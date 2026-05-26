#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <poppler-document.h>
#include <poppler-page.h>

// namespace fs = std::filesystem;

// This HC path will be removed and replaced by read op of the buffer on file selection
const std::string PROJECT_ROOT = "/Users/gaben/dev/personal/pdf-md-bridge";
const std::string OUTPUT_PATH = PROJECT_ROOT + "/output";
const std::string PDF_FILE_PATH = PROJECT_ROOT + "/Gabriel-Mercado-CV.pdf";

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
    }

    // Check metadata in the doc
    int const num_of_pages = doc->pages();

    // Reserve memory
    full_mark_down.reserve(num_of_pages * 3500);

    // Loop pages and extract text
    for (int i = 0; i < num_of_pages; ++i) {
        // Correct RAII management to clean up Poppler heap memory
        std::unique_ptr<const poppler::page> const p(doc->create_page(i));

        if (!p) {
            std::cerr << "Failed to create page " << i << "\n";
            continue;
        }


        // -- EXPERIMENTAL -- Try to use utf 8 for the text and put it in the md
        const poppler::byte_array pageBytes = p->text().to_utf8();
        full_mark_down.append(pageBytes.begin(), pageBytes.end());
        std::ofstream output_file(OUTPUT_PATH + "/doc.md");

        if (output_file.is_open()) {
            output_file << full_mark_down << std::endl;
        }

        // Give back any excess allocated memory
        full_mark_down.shrink_to_fit();
    }


    return 0;
}
