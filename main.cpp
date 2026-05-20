#include <iostream>
#include <filesystem>
#include <poppler-document.h>

namespace fs = std::filesystem;

/*
 * This check_dependencies function should be removed after development since it won't be necessary once compiled,
 * and we want to reduce code to the bare minimum possible
 */
int check_dependencies() {
    try {
        poppler::document *document = poppler::document::load_from_file("/Gabriel-Mercado-CV.pdf");
        std::cout << "Poppler-cpp header found!" << std::endl;
        return 0;
    } catch (const char *msg) {
        // Code to handle the error
        throw std::runtime_error(msg);
    }
}

int main() {
    check_dependencies();

    return 0;
}
