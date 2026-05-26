#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <poppler-document.h>
#include <poppler-page.h>

// namespace fs = std::filesystem;

// This HC path will be removed and replaced by read op of the buffer on file selection
const std::string PROJECT_ROOT = "/Users/gaben/dev/personal/pdf-md-bridge";
const std::string OUTPUT_PATH = PROJECT_ROOT + "/output";
const std::string PDF_FILE_PATH = PROJECT_ROOT + "/Gabriel-Mercado-CV.pdf";

static std::string ustringToUtf8(const poppler::ustring &u) {
    poppler::byte_array bytes = u.to_utf8();
    return {bytes.begin(), bytes.end()};
}

static std::string trim(std::string s) {
    auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && is_ws(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_ws(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

static bool isMostlyUpper(const std::string &s) {
    int letters = 0;
    int upper = 0;
    for (unsigned char c: s) {
        if (std::isalpha(c)) {
            letters++;
            if (std::isupper(c)) upper++;
        }
    }
    if (letters == 0) return false;
    return (static_cast<double>(upper) / static_cast<double>(letters)) > 0.85;
}

static bool looksLikeSectionTitle(const std::string &line) {
    const std::string t = trim(line);
    if (t.size() < 3 || t.size() > 40) return false;
    if (!isMostlyUpper(t)) return false;
    if (t.find('@') != std::string::npos) return false;
    if (t.find("http") != std::string::npos) return false;
    return true;
}

static bool startsWithBulletLike(const std::string &line) {
    std::string t = trim(line);
    if (t.empty()) return false;
    if (t.rfind("·", 0) == 0) return true;
    if (t.rfind("•", 0) == 0) return true;
    if (t.rfind("-", 0) == 0) return true;
    if (t.rfind("–", 0) == 0) return true;
    return false;
}

// Inner Function: Focuses only on loading and validation
std::unique_ptr<poppler::document> loadPdf(const std::string &filePath) {
    // poppler returns raw pointer, wrap it instantly in a smart pointer for RAII
    std::unique_ptr<poppler::document> doc(poppler::document::load_from_file(filePath));

    if (!doc) {
        // Explicitly throw an exception if the file is missing or unreadable
        throw std::runtime_error("Failed to load PDF file. File not found or corrupted: " + filePath);
    }

    return doc;
}

int main() {
    std::unique_ptr<poppler::document> doc;
    std::string full_mark_down;

    // Try to create poppler doc
    try {
        doc = loadPdf(PDF_FILE_PATH);
        std::cout << "Document loaded successfully!\n";
    } catch (const std::exception &err) {
        std::cerr << err.what() << "\n";
        return 1;
    }

    // Check metadata in the doc
    int const num_of_pages = doc->pages();

    // Reserve memory
    // full_mark_down.reserve(num_of_pages * 3500);
    full_mark_down.reserve(static_cast<size_t>(doc->pages()) * 8192);

    // A simple structure to represent a row of text based on Y-coordinates
    struct TextLine {
        double y_coord;
        std::vector<poppler::text_box> boxes;
    };

    // Loop pages and extract text
    for (int i = 0; i < num_of_pages; ++i) {
        // Correct RAII management to clean up Poppler heap memory
        std::unique_ptr<const poppler::page> const page(doc->create_page(i));

        if (!page) {
            std::cerr << "Failed to create page " << i << "\n";
            continue;
        }

        // Optional page marker; keep minimal so output stays usable as a single document.
        // full_mark_down += "## Page " + std::to_string(i + 1) + "\n\n";

        // 1. Get all individual text boxes with coordinates
        std::vector<poppler::text_box> raw_boxes = page->text_list();
        std::vector<TextLine> lines;

        // 2. Group boxes into horizontal lines based on Y-coordinates
        // PDF coordinates often have tiny mathematical rounding differences,
        // so we treat boxes within 4 pixels of height as being on the same line.
        const double Y_THRESHOLD = 2.5;

        for (auto &box: raw_boxes) {
            bool found_line = false;
            double box_y = box.bbox().y();

            for (auto &line: lines) {
                if (std::abs(line.y_coord - box_y) < Y_THRESHOLD) {
                    line.boxes.push_back(std::move(box));
                    found_line = true;
                    break;
                }
            }

            if (!found_line) {
                // Brace-init {std::move(box)} uses initializer_list, which copies.
                TextLine new_line{box_y, {}};
                new_line.boxes.push_back(std::move(box));
                lines.push_back(std::move(new_line));
            }
        }

        // 3. Sort lines vertically (Top of the page to Bottom)
        std::sort(lines.begin(), lines.end(), [](const TextLine &a, const TextLine &b) {
            return a.y_coord < b.y_coord;
        });

        // 4. Sort boxes within each line horizontally (Left to Right)
        for (auto &line: lines) {
            std::sort(line.boxes.begin(), line.boxes.end(),
                      [](const poppler::text_box &a, const poppler::text_box &b) {
                          return a.bbox().x() < b.bbox().x();
                      });

            // 5. Reconstruct the text line with proper spacing
            std::string line_text;
            line_text.reserve(128);
            for (const auto &box: line.boxes) {
                line_text += ustringToUtf8(box.text());
                if (box.has_space_after()) line_text += ' ';
            }
            line_text = trim(line_text);
            full_mark_down += line_text;
            full_mark_down += "\n";
        }
        full_mark_down += "\n";
    }

    // Post-process line-by-line to infer headings + lists (cheap heuristics).
    {
        std::string out;
        out.reserve(full_mark_down.size() + 1024);

        std::filesystem::create_directories(OUTPUT_PATH);

        std::string line;
        line.reserve(256);
        bool prev_blank = true;

        auto flush_line = [&](const std::string &ln) {
            const std::string t = trim(ln);
            if (t.empty()) {
                if (!prev_blank) out += "\n";
                prev_blank = true;
                return;
            }

            if (looksLikeSectionTitle(t)) {
                if (!prev_blank) out += "\n";
                out += "## " + t + "\n\n";
                prev_blank = true;
                return;
            }

            if (startsWithBulletLike(t)) {
                std::string item = trim(t);
                if (item.rfind("·", 0) == 0 || item.rfind("•", 0) == 0) item = trim(item.substr(2));
                if (item.rfind("-", 0) == 0) item = trim(item.substr(1));
                if (item.rfind("–", 0) == 0) item = trim(item.substr(3));
                out += "- " + item + "\n";
                prev_blank = false;
                return;
            }

            // Bold obvious "Label: value" pairs.
            const auto colon = t.find(':');
            if (colon != std::string::npos && colon > 1 && colon < 20) {
                std::string label = trim(t.substr(0, colon));
                std::string value = trim(t.substr(colon + 1));
                if (!label.empty() && !value.empty()) {
                    out += "**" + label + ":** " + value + "\n";
                    prev_blank = false;
                    return;
                }
            }

            out += t + "\n";
            prev_blank = false;
        };

        for (char c: full_mark_down) {
            if (c == '\n') {
                flush_line(line);
                line.clear();
            } else {
                line.push_back(c);
            }
        }
        flush_line(line);

        full_mark_down.swap(out);
    }

    std::ofstream outFile(OUTPUT_PATH + "/doc.md");

    if (outFile.is_open()) {
        outFile << full_mark_down;
        outFile.close();
    }

    return 0;
}
