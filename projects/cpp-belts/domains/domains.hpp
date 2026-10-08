#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace museum::domains {

class Filter {
public:
    explicit Filter(std::vector<std::string> banned) {
        for (auto& domain : banned) {
            Validate(domain);
            std::reverse(domain.begin(), domain.end());
            domain.push_back('.');  // The boundary is part of the prefix.
        }
        std::sort(banned.begin(), banned.end());
        roots_.reserve(banned.size());
        for (auto& domain : banned) {
            if (roots_.empty() || !IsPrefix(roots_.back(), domain)) {
                roots_.push_back(std::move(domain));
            }
        }
    }

    bool IsBlocked(std::string_view domain) const {
        Validate(domain);
        std::string reversed(domain.rbegin(), domain.rend());
        reversed.push_back('.');
        const auto after = std::upper_bound(roots_.begin(), roots_.end(), reversed);
        return after != roots_.begin() && IsPrefix(*std::prev(after), reversed);
    }

    std::size_t RootCount() const noexcept { return roots_.size(); }

private:
    static bool IsPrefix(std::string_view prefix, std::string_view word) noexcept {
        return word.size() >= prefix.size() && word.substr(0, prefix.size()) == prefix;
    }

    static void Validate(std::string_view domain) {
        if (domain.empty() || domain.front() == '.' || domain.back() == '.') {
            throw std::invalid_argument("expected nonempty lowercase domain labels");
        }
        char previous = '.';
        for (const char letter : domain) {
            if ((letter == '.' && previous == '.') ||
                (letter != '.' && (letter < 'a' || letter > 'z'))) {
                throw std::invalid_argument("expected lowercase letters separated by single dots");
            }
            previous = letter;
        }
    }

    std::vector<std::string> roots_;
};

}  // namespace museum::domains
