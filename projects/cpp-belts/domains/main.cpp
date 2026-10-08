#include "domains.hpp"

#include <charconv>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string ReadToken() {
    std::string token;
    if (!(std::cin >> token)) throw std::runtime_error("incomplete input");
    return token;
}

std::size_t ReadCount() {
    const auto token = ReadToken();
    std::size_t count = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), count);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || count > 10000) {
        throw std::invalid_argument("expected a count from 0 to 10000");
    }
    return count;
}

std::string ReadDomain() {
    auto domain = ReadToken();
    if (domain.size() > 50) throw std::invalid_argument("domain exceeds 50 characters");
    return domain;
}

}  // namespace

int main() {
    try {
        const auto count = ReadCount();
        std::vector<std::string> banned;
        banned.reserve(count);
        for (std::size_t i = 0; i < count; ++i) banned.push_back(ReadDomain());
        const museum::domains::Filter filter(std::move(banned));
        const auto query_count = ReadCount();
        for (std::size_t i = 0; i < query_count; ++i) {
            std::cout << (filter.IsBlocked(ReadDomain()) ? "Bad\n" : "Good\n");
        }
        std::string extra;
        if (std::cin >> extra) throw std::invalid_argument("unexpected trailing input");
        if (std::cin.bad()) throw std::runtime_error("failed to read input");
        if (!std::cout) throw std::runtime_error("failed to write output");
    } catch (const std::exception& error) {
        std::cerr << "domains: " << error.what() << '\n';
        return 1;
    }
}
