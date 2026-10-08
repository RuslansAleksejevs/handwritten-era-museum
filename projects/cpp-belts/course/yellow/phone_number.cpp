#include "phone_number.h"
#include <stdexcept>
namespace yellow {
PhoneNumber::PhoneNumber(const std::string &s) {
    auto a = s.find('-'), b = a == s.npos ? s.npos : s.find('-', a + 1);
    if (s.empty() || s.front() != '+' || a == s.npos || a <= 1 || b == s.npos || b == a + 1 ||
        b + 1 == s.size())
        throw std::invalid_argument("invalid international phone number");
    country_code_ = s.substr(1, a - 1);
    city_code_ = s.substr(a + 1, b - a - 1);
    local_number_ = s.substr(b + 1);
}
std::string PhoneNumber::GetCountryCode() const { return country_code_; }
std::string PhoneNumber::GetCityCode() const { return city_code_; }
std::string PhoneNumber::GetLocalNumber() const { return local_number_; }
std::string PhoneNumber::GetInternationalNumber() const {
    return "+" + country_code_ + "-" + city_code_ + "-" + local_number_;
}
} // namespace yellow
