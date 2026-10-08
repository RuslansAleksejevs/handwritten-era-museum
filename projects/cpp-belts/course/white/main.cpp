#include "commands.hpp"
#include <fstream>
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: white <exercise>\n";
        return 2;
    }
    try {
        const std::string name = argv[1];
        if (name == "copy" || name == "copy-file" || name == "precision" || name == "table") {
            std::ifstream input("input.txt", std::ios::binary);
            if (!input)
                throw std::runtime_error("cannot open input.txt");
            if (name == "copy-file") {
                std::ofstream output("output.txt", std::ios::binary);
                if (!output)
                    throw std::runtime_error("cannot open output.txt");
                white::Run(name, input, output);
            } else
                white::Run(name, input, std::cout);
        } else
            white::Run(name, std::cin, std::cout);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
