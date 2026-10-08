#include "commands.hpp"
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: yellow <exercise>\n";
        return 2;
    }
    try {
        yellow::Run(argv[1], std::cin, std::cout);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
