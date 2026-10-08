#include "date.h"
#include "../../white/date.hpp"
namespace yellow::events {
Date::Date(int y, int m, int d) : year_(y), month_(m), day_(d) {
    if (y < 0 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
        throw std::invalid_argument("invalid event date");
}
Date ParseDate(std::istream &in) {
    const auto d = white::ParseDate(in);
    return {d.year, d.month, d.day};
}
std::ostream &operator<<(std::ostream &out, const Date &d) {
    return out << white::Date{d.GetYear(), d.GetMonth(), d.GetDay()};
}
} // namespace yellow::events
