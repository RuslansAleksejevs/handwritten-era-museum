#pragma once
#include <string>
namespace yellow {
class PhoneNumber {
    std::string country_code_, city_code_, local_number_;

  public:
    explicit PhoneNumber(const std::string &);
    std::string GetCountryCode() const;
    std::string GetCityCode() const;
    std::string GetLocalNumber() const;
    std::string GetInternationalNumber() const;
};
} // namespace yellow
