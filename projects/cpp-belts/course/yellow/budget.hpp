#pragma once
#include "../white/date.hpp"
#include <cstdint>
#include <numeric>
#include <vector>
namespace yellow {
inline bool Leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
inline int DayIndex(const white::Date &d) {
    if (d.year < 1700 || d.year > 2099)
        throw std::out_of_range("budget year outside 1700..2099");
    constexpr int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (d.month < 1 || d.month > 12 || d.day < 1 ||
        d.day > lengths[d.month - 1] + (d.month == 2 && Leap(d.year)))
        throw std::invalid_argument("invalid calendar date");
    const int y = d.year - 1, base = 1699;
    int n = 365 * (y - base) + (y / 4 - base / 4) - (y / 100 - base / 100) + (y / 400 - base / 400);
    for (int m = 1; m < d.month; ++m)
        n += lengths[m - 1] + (m == 2 && Leap(d.year));
    return n + d.day - 1;
}
class Budget {
    std::vector<long double> days_ = std::vector<long double>(146097);

  public:
    void Earn(const white::Date &from, const white::Date &to, long double value) {
        int a = DayIndex(from), b = DayIndex(to);
        if (a > b)
            throw std::invalid_argument("reversed date interval");
        const auto per_day = value / (b - a + 1);
        for (int i = a; i <= b; ++i)
            days_[i] += per_day;
    }
    long double ComputeIncome(const white::Date &from, const white::Date &to) const {
        int a = DayIndex(from), b = DayIndex(to);
        if (a > b)
            throw std::invalid_argument("reversed date interval");
        return std::accumulate(days_.begin() + a, days_.begin() + b + 1, 0.L);
    }
};
class PrefixBudget {
    std::vector<std::int64_t> prefix_ = std::vector<std::int64_t>(146098);
    bool sealed_ = false;

  public:
    void Earn(const white::Date &date, std::int64_t value) {
        if (sealed_)
            throw std::logic_error("earnings already sealed");
        prefix_[DayIndex(date) + 1] += value;
    }
    void Seal() {
        if (!sealed_) {
            std::partial_sum(prefix_.begin(), prefix_.end(), prefix_.begin());
            sealed_ = true;
        }
    }
    std::int64_t ComputeIncome(const white::Date &from, const white::Date &to) const {
        if (!sealed_)
            throw std::logic_error("seal earnings before querying");
        int a = DayIndex(from), b = DayIndex(to);
        if (a > b)
            throw std::invalid_argument("reversed date interval");
        return prefix_[b + 1] - prefix_[a];
    }
};
} // namespace yellow
