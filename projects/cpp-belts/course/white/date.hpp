#pragma once
#include <iomanip>
#include <istream>
#include <ostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <tuple>
namespace white {
struct Date {
    int year = 0, month = 1, day = 1;
    friend bool operator<(const Date &a, const Date &b) {
        return std::tie(a.year, a.month, a.day) < std::tie(b.year, b.month, b.day);
    }
    friend bool operator==(const Date &a, const Date &b) {
        return std::tie(a.year, a.month, a.day) == std::tie(b.year, b.month, b.day);
    }
};
inline Date ParseDate(const std::string &s) {
    static const std::regex form(R"(([+-]?[0-9]+)-([+-]?[0-9]+)-([+-]?[0-9]+))");
    std::smatch match;
    Date d;
    if (!std::regex_match(s, match, form))
        throw std::invalid_argument("Wrong date format: " + s);
    try {
        d = {std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3])};
    } catch (const std::exception &) {
        throw std::invalid_argument("Wrong date format: " + s);
    }
    if (d.month < 1 || d.month > 12)
        throw std::invalid_argument("Month value is invalid: " + std::to_string(d.month));
    if (d.day < 1 || d.day > 31)
        throw std::invalid_argument("Day value is invalid: " + std::to_string(d.day));
    return d;
}
inline Date ParseDate(std::istream &in) {
    std::string s;
    in >> s;
    return ParseDate(s);
}
inline std::ostream &operator<<(std::ostream &out, const Date &d) {
    const auto fill = out.fill('0');
    out << std::setw(4) << d.year << '-' << std::setw(2) << d.month << '-' << std::setw(2) << d.day;
    out.fill(fill);
    return out;
}
} // namespace white
