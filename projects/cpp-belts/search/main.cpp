#include "search.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: search DOCUMENTS_FILE < QUERIES_FILE\n";
        return 2;
    }
    try {
        std::ifstream documents(argv[1]);
        if (!documents) throw std::runtime_error("cannot open documents file");
        const museum::search::SearchServer server(documents);
        server.AddQueriesStream(std::cin, std::cout);
    } catch (const std::exception& error) {
        std::cerr << "search: " << error.what() << '\n';
        return 1;
    }
}
