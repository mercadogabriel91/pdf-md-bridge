#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <poppler-document.h>
#include <poppler-page.h>

// This HC path will be removed and replaced by read op of the buffer on file selection
const std::string PROJECT_ROOT = "/Users/gaben/dev/personal/pdf-md-bridge";
const std::string OUTPUT_PATH = PROJECT_ROOT + "/output";
const std::string PDF_FILE_PATH = PROJECT_ROOT + "/Gabriel-Mercado-CV-original.pdf";

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

static std::string toLowerAscii(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c: s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

// Subset-embedded font names look like "ABCDEF+Helvetica-Bold".
// Strip the 6-uppercase-letter prefix + '+' if present.
static std::string stripFontSubsetPrefix(const std::string &name) {
    if (name.size() > 7 && name[6] == '+') {
        for (int i = 0; i < 6; ++i) {
            if (!std::isupper(static_cast<unsigned char>(name[i]))) return name;
        }
        return name.substr(7);
    }
    return name;
}

static bool fontIsBold(const std::string &font_name) {
    const std::string n = toLowerAscii(stripFontSubsetPrefix(font_name));
    return n.find("bold") != std::string::npos
           || n.find("black") != std::string::npos
           || n.find("heavy") != std::string::npos;
}

static bool fontIsItalic(const std::string &font_name) {
    const std::string n = toLowerAscii(stripFontSubsetPrefix(font_name));
    return n.find("italic") != std::string::npos || n.find("oblique") != std::string::npos;
}

// UTF-8 leading bullet glyphs we want to strip, with their exact byte lengths.
struct BulletGlyph {
    const char *utf8;
    size_t len;
    int level; // nesting hint (0 = outer, 1 = inner, 2 = innermost)
};

static constexpr BulletGlyph BULLETS[] = {
    {"\xE2\x97\x8F", 3, 0}, // ●
    {"\xE2\x80\xA2", 3, 0}, // •
    {"\xC2\xB7", 2, 0}, // ·
    {"\xE2\x97\x8B", 3, 1}, // ○
    {"\xE2\x97\xA6", 3, 1}, // ◦
    {"\xE2\x96\xA0", 3, 2}, // ■
    {"\xE2\x96\xAA", 3, 2}, // ▪
    {"\xE2\x96\xA1", 3, 2}, // □
    {"\xE2\x80\x93", 3, 0}, // –
    {"\xE2\x80\x94", 3, 0}, // —
};

// If `s` begins with a recognized bullet glyph, fill `body` with the rest
// (trimmed) and `level` with the nesting hint, and return true.
// ASCII '-' counts only when followed by whitespace (so we don't mangle words).
static bool stripLeadingBullet(const std::string &s, std::string &body, int &level) {
    for (const auto &b: BULLETS) {
        if (s.size() >= b.len && std::memcmp(s.data(), b.utf8, b.len) == 0) {
            body = trim(s.substr(b.len));
            level = b.level;
            return true;
        }
    }
    if (!s.empty() && s.front() == '-' &&
        (s.size() == 1 || std::isspace(static_cast<unsigned char>(s[1])))) {
        body = trim(s.substr(1));
        level = 0;
        return true;
    }
    return false;
}

static bool isPureDigits(const std::string &s) {
    if (s.empty()) return false;
    for (unsigned char c: s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

// Round to nearest 0.5pt so near-identical font sizes bucket together.
static double bucketFontSize(double size) {
    return std::round(size * 2.0) / 2.0;
}

// Inner Function: Focuses only on loading and validation
std::unique_ptr<poppler::document> loadPdf(const std::string &filePath) {
    std::unique_ptr<poppler::document> doc(poppler::document::load_from_file(filePath));
    if (!doc) {
        throw std::runtime_error("Failed to load PDF file. File not found or corrupted: " + filePath);
    }
    return doc;
}

struct Token {
    std::string text;
    bool has_space_after = false;
    double font_size = 0.0;
    bool bold = false;
    bool italic = false;
    double x = 0.0;
};

struct Line {
    double y_coord = 0.0;
    double page_height = 0.0;
    int page_index = 0;
    std::vector<Token> tokens;
    double dominant_font_size = 0.0;
};

static std::string lineText(const Line &line) {
    std::string s;
    s.reserve(128);
    for (const auto &t: line.tokens) {
        s += t.text;
        if (t.has_space_after) s += ' ';
    }
    return trim(s);
}

// Render a line preserving per-run bold spans. Trailing whitespace before a
// closing `**` is stripped to satisfy CommonMark's "right-flanking" rule.
static std::string renderLineWithEmphasis(const Line &line) {
    std::string out;
    out.reserve(256);
    bool in_bold = false;
    bool prev_space_after = false;
    for (const auto &t: line.tokens) {
        const bool is_bold = t.bold && !t.text.empty();
        if (is_bold && !in_bold) {
            out += "**";
            in_bold = true;
        } else if (!is_bold && in_bold) {
            while (!out.empty() && out.back() == ' ') out.pop_back();
            out += "**";
            if (prev_space_after) out += ' ';
            in_bold = false;
        }
        out += t.text;
        if (t.has_space_after) out += ' ';
        prev_space_after = t.has_space_after;
    }
    if (in_bold) {
        while (!out.empty() && out.back() == ' ') out.pop_back();
        out += "**";
    }
    return trim(out);
}

static std::string normalizeForFurniture(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c: s) {
        if (std::isspace(c)) continue;
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

int main() {
    std::unique_ptr<poppler::document> doc;
    try {
        doc = loadPdf(PDF_FILE_PATH);
        std::cout << "Document loaded successfully!\n";
    } catch (const std::exception &err) {
        std::cerr << err.what() << "\n";
        return 1;
    }

    const int num_of_pages = doc->pages();

    // Boxes within this many points of each other vertically count as one line.
    // Slightly looser than before to absorb wobble in larger heading glyphs.
    constexpr double Y_THRESHOLD = 3.0;

    // ----- Pass 1: extract every line with font metadata -----
    std::vector<std::vector<Line> > pages_lines;
    pages_lines.reserve(num_of_pages);

    for (int i = 0; i < num_of_pages; ++i) {
        std::unique_ptr<const poppler::page> const page(doc->create_page(i));
        if (!page) {
            std::cerr << "Failed to create page " << i << "\n";
            pages_lines.emplace_back();
            continue;
        }

        const double page_height = page->page_rect().height();

        std::vector<poppler::text_box> raw_boxes =
                page->text_list(poppler::page::text_list_include_font);

        // Group box indexes by Y coordinate.
        struct Group {
            double y = 0.0;
            std::vector<size_t> idx;
        };
        std::vector<Group> groups;
        groups.reserve(raw_boxes.size());
        for (size_t k = 0; k < raw_boxes.size(); ++k) {
            const double box_y = raw_boxes[k].bbox().y();
            bool placed = false;
            for (auto &g: groups) {
                if (std::abs(g.y - box_y) < Y_THRESHOLD) {
                    g.idx.push_back(k);
                    placed = true;
                    break;
                }
            }
            if (!placed) groups.push_back({box_y, {k}});
        }

        std::sort(groups.begin(), groups.end(),
                  [](const Group &a, const Group &b) { return a.y < b.y; });

        std::vector<Line> lines;
        lines.reserve(groups.size());

        for (auto &g: groups) {
            std::sort(g.idx.begin(), g.idx.end(),
                      [&](size_t a, size_t b) {
                          return raw_boxes[a].bbox().x() < raw_boxes[b].bbox().x();
                      });

            Line line;
            line.y_coord = g.y;
            line.page_height = page_height;
            line.page_index = i;
            line.tokens.reserve(g.idx.size());

            // Pick the dominant font size weighted by character count.
            std::unordered_map<double, long long> size_weight;

            for (size_t k: g.idx) {
                poppler::text_box &b = raw_boxes[k];
                Token t;
                t.text = ustringToUtf8(b.text());
                t.has_space_after = b.has_space_after();
                t.x = b.bbox().x();
                if (b.has_font_info()) {
                    t.font_size = b.get_font_size();
                    const std::string fname = b.get_font_name(0);
                    t.bold = fontIsBold(fname);
                    t.italic = fontIsItalic(fname);
                }
                if (t.font_size > 0.0) {
                    size_weight[bucketFontSize(t.font_size)] +=
                            static_cast<long long>(t.text.size());
                }
                line.tokens.push_back(std::move(t));
            }

            double best_size = 0.0;
            long long best_weight = -1;
            for (const auto &[sz, w]: size_weight) {
                if (w > best_weight) {
                    best_weight = w;
                    best_size = sz;
                }
            }
            line.dominant_font_size = best_size;

            lines.push_back(std::move(line));
        }

        pages_lines.push_back(std::move(lines));
    }

    // ----- Pass 2: derive body font size, heading buckets, and repeating furniture -----
    std::unordered_map<double, long long> size_chars;
    for (const auto &lines: pages_lines) {
        for (const auto &line: lines) {
            if (line.dominant_font_size <= 0.0) continue;
            long long chars = 0;
            for (const auto &t: line.tokens) chars += static_cast<long long>(t.text.size());
            size_chars[line.dominant_font_size] += chars;
        }
    }

    double body_size = 0.0;
    long long body_weight = -1;
    for (const auto &[sz, w]: size_chars) {
        if (w > body_weight) {
            body_weight = w;
            body_size = sz;
        }
    }

    std::vector<double> heading_sizes;
    heading_sizes.reserve(size_chars.size());
    for (const auto &[sz, _w]: size_chars) {
        if (sz > body_size + 0.25) heading_sizes.push_back(sz);
    }
    std::sort(heading_sizes.begin(), heading_sizes.end(), std::greater<double>());

    auto headingLevel = [&](double size) -> int {
        for (size_t k = 0; k < heading_sizes.size(); ++k) {
            if (std::abs(size - heading_sizes[k]) < 0.25) {
                return static_cast<int>(std::min<size_t>(k + 1, 6));
            }
        }
        return 0;
    };

    // Repeating header/footer detection: only lines sitting in the top or
    // bottom 15% of their page count toward the tally.
    std::unordered_map<std::string, std::unordered_set<int> > furniture_pages;
    for (const auto &lines: pages_lines) {
        if (lines.empty()) continue;
        const double ph = lines.front().page_height;
        const double top_band = ph * 0.15;
        const double bot_band = ph * 0.85;
        for (const auto &line: lines) {
            const bool in_band = line.y_coord <= top_band || line.y_coord >= bot_band;
            if (!in_band) continue;
            const std::string txt = lineText(line);
            if (txt.empty()) continue;
            const std::string key = normalizeForFurniture(txt);
            if (key.empty()) continue;
            furniture_pages[key].insert(line.page_index);
        }
    }

    // Need a repeat on >= 3 pages, but degrade gracefully on tiny docs.
    const int furniture_threshold =
            std::max(2, std::min(3, (num_of_pages + 1) / 2));
    std::unordered_set<std::string> furniture_keys;
    for (const auto &[k, set]: furniture_pages) {
        if (static_cast<int>(set.size()) >= furniture_threshold) {
            furniture_keys.insert(k);
        }
    }

    // ----- Pass 3: emit Markdown -----
    std::string md;
    md.reserve(static_cast<size_t>(num_of_pages) * 8192);
    bool prev_blank = true;

    auto emitBlank = [&]() {
        if (!prev_blank) {
            md += "\n";
            prev_blank = true;
        }
    };

    for (const auto &lines: pages_lines) {
        for (const auto &line: lines) {
            const std::string text = lineText(line);
            if (text.empty()) {
                emitBlank();
                continue;
            }

            if (furniture_keys.count(normalizeForFurniture(text))) continue;
            if (isPureDigits(text) && text.size() <= 4) continue;

            std::string bullet_body;
            int bullet_level = 0;
            const bool is_bullet = stripLeadingBullet(text, bullet_body, bullet_level);

            const int hlevel = headingLevel(line.dominant_font_size);

            if (hlevel > 0 && !is_bullet) {
                emitBlank();
                md += std::string(static_cast<size_t>(hlevel), '#');
                md += ' ';
                md += text;
                md += "\n\n";
                prev_blank = true;
                continue;
            }

            std::string rendered = renderLineWithEmphasis(line);

            if (is_bullet) {
                // Strip the bullet glyph off the rendered (emphasis-aware) form.
                std::string stripped;
                int dummy = 0;
                if (stripLeadingBullet(rendered, stripped, dummy)) rendered = stripped;
                md += std::string(static_cast<size_t>(bullet_level) * 2, ' ');
                md += "- ";
                md += rendered;
                md += "\n";
                prev_blank = false;
                continue;
            }

            md += rendered;
            md += "\n";
            prev_blank = false;
        }
        emitBlank();
    }

    std::filesystem::create_directories(OUTPUT_PATH);
    std::ofstream outFile(OUTPUT_PATH + "/doc.md");
    if (outFile.is_open()) {
        outFile << md;
        outFile.close();
    }

    return 0;
}
