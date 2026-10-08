#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace white {
inline std::vector<double> RealRoots(double a, double b, double c) {
    if (a == 0)
        return b == 0 ? std::vector<double>{} : std::vector<double>{-c / b};
    const long double d = static_cast<long double>(b) * b - 4.L * a * c;
    if (d < 0)
        return {};
    if (d == 0)
        return {-b / (2 * a)};
    const long double q = -.5L * (b + std::copysign(std::sqrt(d), b));
    return {static_cast<double>(q / a), static_cast<double>(c / q)};
}
inline int Factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i)
        result *= i;
    return result;
}
inline bool IsPalindrom(const std::string &s) {
    return std::equal(s.begin(), s.begin() + s.size() / 2, s.rbegin());
}
inline std::vector<std::string> PalindromFilter(const std::vector<std::string> &words,
                                                int minimum) {
    std::vector<std::string> result;
    for (const auto &word : words)
        if (static_cast<int>(word.size()) >= minimum && IsPalindrom(word))
            result.push_back(word);
    return result;
}
inline void UpdateIfGreater(int first, int &second) { second = std::max(first, second); }
inline void MoveStrings(std::vector<std::string> &source, std::vector<std::string> &destination) {
    if (&source == &destination)
        throw std::invalid_argument("distinct vectors required");
    destination.insert(destination.end(), std::make_move_iterator(source.begin()),
                       std::make_move_iterator(source.end()));
    source.clear();
}
inline void Reverse(std::vector<int> &values) { std::reverse(values.begin(), values.end()); }
inline std::vector<int> Reversed(const std::vector<int> &values) {
    return {values.rbegin(), values.rend()};
}
inline std::vector<std::size_t> AboveAverage(const std::vector<int> &values) {
    if (values.empty())
        return {};
    const auto sum = std::accumulate(values.begin(), values.end(), std::int64_t{0});
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < values.size(); ++i)
        if (static_cast<std::int64_t>(values[i]) * static_cast<std::int64_t>(values.size()) > sum)
            result.push_back(i);
    return result;
}
inline std::map<char, int> BuildCharCounters(const std::string &word) {
    std::map<char, int> result;
    for (char c : word)
        ++result[c];
    return result;
}
inline std::set<std::string> BuildMapValuesSet(const std::map<int, std::string> &values) {
    std::set<std::string> result;
    for (const auto &item : values)
        result.insert(item.second);
    return result;
}
class SortedStrings {
    std::multiset<std::string> strings_;

  public:
    void AddString(const std::string &s) { strings_.insert(s); }
    std::vector<std::string> GetSortedStrings() const { return {strings_.begin(), strings_.end()}; }
};
class ReversibleString {
    std::string value_;

  public:
    ReversibleString() = default;
    explicit ReversibleString(std::string value) : value_(std::move(value)) {}
    void Reverse() { std::reverse(value_.begin(), value_.end()); }
    std::string ToString() const { return value_; }
};
struct Incognizable {
    int first = 0;
    int second = 0;
};
struct Specialization {
    std::string value;
    explicit Specialization(std::string s) : value(std::move(s)) {}
};
struct Course {
    std::string value;
    explicit Course(std::string s) : value(std::move(s)) {}
};
struct Week {
    std::string value;
    explicit Week(std::string s) : value(std::move(s)) {}
};
struct LectureTitle {
    std::string specialization, course, week;
    LectureTitle(Specialization s, Course c, Week w)
        : specialization(std::move(s.value)), course(std::move(c.value)), week(std::move(w.value)) {
    }
};
class Function {
    std::vector<std::pair<char, double>> parts_;

  public:
    void AddPart(char op, double value) {
        if (std::string("+-*/").find(op) == std::string::npos ||
            ((op == '*' || op == '/') && value == 0))
            throw std::invalid_argument("operation must be invertible");
        parts_.emplace_back(op, value);
    }
    double Apply(double value) const {
        for (auto [op, x] : parts_) {
            if (op == '+')
                value += x;
            else if (op == '-')
                value -= x;
            else if (op == '*')
                value *= x;
            else
                value /= x;
        }
        return value;
    }
    void Invert() {
        std::reverse(parts_.begin(), parts_.end());
        for (auto &part : parts_) {
            const char c = part.first;
            part.first = c == '+' ? '-' : c == '-' ? '+' : c == '*' ? '/' : '*';
        }
    }
};
inline void EnsureEqual(const std::string &a, const std::string &b) {
    if (a != b)
        throw std::runtime_error(a + " != " + b);
}
// Supplied by the embedding program; tests provide a deterministic fake.
std::string AskTimeServer();
class TimeServer {
    std::string LastFetchedTime = "00:00:00";

  public:
    std::string GetCurrentTime() {
        try {
            LastFetchedTime = AskTimeServer();
        } catch (const std::system_error &) {
        }
        return LastFetchedTime;
    }
};
class MonthTasks {
    int month_ = 0;
    std::vector<std::vector<std::string>> days_{31};

  public:
    void Add(int day, std::string task) { days_.at(day - 1).push_back(std::move(task)); }
    const std::vector<std::string> &Get(int day) const { return days_.at(day - 1); }
    void Next() {
        constexpr int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        month_ = (month_ + 1) % 12;
        const auto size = static_cast<std::size_t>(lengths[month_]);
        for (std::size_t i = size; i < days_.size(); ++i)
            days_[size - 1].insert(days_[size - 1].end(), days_[i].begin(), days_[i].end());
        days_.resize(size);
    }
};
} // namespace white
