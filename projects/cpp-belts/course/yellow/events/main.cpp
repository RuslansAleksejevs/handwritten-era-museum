#include "database.h"
#include <iostream>
int main() {
    try {
        yellow::events::Run(std::cin, std::cout);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
