#pragma once
#include <istream>
#include <ostream>
#include <tuple>
namespace yellow::events {
class Date {
    int year_, month_, day_;

  public:
    Date(int year, int month, int day);
    int GetYear() const { return year_; }
    int GetMonth() const { return month_; }
    int GetDay() const { return day_; }
    friend bool operator<(const Date &a, const Date &b) {
        return std::tie(a.year_, a.month_, a.day_) < std::tie(b.year_, b.month_, b.day_);
    }
    friend bool operator==(const Date &a, const Date &b) {
        return std::tie(a.year_, a.month_, a.day_) == std::tie(b.year_, b.month_, b.day_);
    }
};
Date ParseDate(std::istream &);
std::ostream &operator<<(std::ostream &, const Date &);
} // namespace yellow::events
