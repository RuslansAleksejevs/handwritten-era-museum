#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace museum::brown {
// Date arithmetic is independent of local time zones and daylight-saving changes.
inline int DayIndex(const std::string &text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-')
        throw std::invalid_argument("expected YYYY-MM-DD");
    auto number = [&](int start, int length) {
        int n = 0;
        for (int i = 0; i < length; ++i) {
            char c = text[start + i];
            if (c < '0' || c > '9')
                throw std::invalid_argument("invalid date digit");
            n = n * 10 + c - '0';
        }
        return n;
    };
    const int y = number(0, 4), m = number(5, 2), d = number(8, 2);
    if (y < 2000 || y > 2099 || m < 1 || m > 12)
        throw std::out_of_range("date outside 2000–2099");
    static constexpr std::array<int, 12> lengths{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    if (d < 1 || d > lengths[m - 1] + (m == 2 && leap))
        throw std::invalid_argument("invalid day");
    int days = (y - 2000) * 365 + (y - 1997) / 4;
    for (int month = 1; month < m; ++month)
        days += lengths[month - 1] + (month == 2 && leap);
    return days + d - 1;
}
class Budget {
    static constexpr int kDays = 36525;
    struct Node {
        double earned = 0, spent = 0, multiply = 1, add_earned = 0, add_spent = 0;
    };
    std::vector<Node> tree_ = std::vector<Node>(4 * kDays);
    void Apply(int index, int length, double mul, double earn, double spend) {
        auto &n = tree_[index];
        n.earned = n.earned * mul + earn * length;
        n.spent += spend * length;
        n.multiply *= mul;
        n.add_earned = n.add_earned * mul + earn;
        n.add_spent += spend;
    }
    void Push(int index, int left, int right) {
        if (right - left == 1)
            return;
        auto &n = tree_[index];
        const int middle = (left + right) / 2;
        Apply(index * 2, middle - left, n.multiply, n.add_earned, n.add_spent);
        Apply(index * 2 + 1, right - middle, n.multiply, n.add_earned, n.add_spent);
        n.multiply = 1;
        n.add_earned = n.add_spent = 0;
    }
    void Update(int index, int left, int right, int from, int to, double mul, double earn,
                double spend) {
        if (to <= left || right <= from)
            return;
        if (from <= left && right <= to) {
            Apply(index, right - left, mul, earn, spend);
            return;
        }
        Push(index, left, right);
        const int middle = (left + right) / 2;
        Update(index * 2, left, middle, from, to, mul, earn, spend);
        Update(index * 2 + 1, middle, right, from, to, mul, earn, spend);
        tree_[index].earned = tree_[index * 2].earned + tree_[index * 2 + 1].earned;
        tree_[index].spent = tree_[index * 2].spent + tree_[index * 2 + 1].spent;
    }
    double Sum(int index, int left, int right, int from, int to) {
        if (to <= left || right <= from)
            return 0;
        if (from <= left && right <= to)
            return tree_[index].earned - tree_[index].spent;
        Push(index, left, right);
        const int middle = (left + right) / 2;
        return Sum(index * 2, left, middle, from, to) + Sum(index * 2 + 1, middle, right, from, to);
    }
    static std::pair<int, int> Range(const std::string &from, const std::string &to) {
        auto a = DayIndex(from), b = DayIndex(to) + 1;
        if (a >= b)
            throw std::invalid_argument("reversed dates");
        return {a, b};
    }

  public:
    void Earn(const std::string &from, const std::string &to, double amount) {
        auto [a, b] = Range(from, to);
        Update(1, 0, kDays, a, b, 1, amount / (b - a), 0);
    }
    void Spend(const std::string &from, const std::string &to, double amount) {
        auto [a, b] = Range(from, to);
        Update(1, 0, kDays, a, b, 1, 0, amount / (b - a));
    }
    void PayTax(const std::string &from, const std::string &to, double percent = 13) {
        auto [a, b] = Range(from, to);
        if (percent < 0 || percent > 100)
            throw std::invalid_argument("tax outside 0–100");
        Update(1, 0, kDays, a, b, 1 - percent / 100, 0, 0);
    }
    double ComputeIncome(const std::string &from, const std::string &to) {
        auto [a, b] = Range(from, to);
        return Sum(1, 0, kDays, a, b);
    }
};
} // namespace museum::brown
