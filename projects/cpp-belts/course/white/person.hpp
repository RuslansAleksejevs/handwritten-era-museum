#pragma once
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace white {
class Person {
    std::map<int, std::string> first_, last_;

  protected:
    int birth_ = std::numeric_limits<int>::min();

  private:
    static std::string Name(const std::map<int, std::string> &changes, int year, bool history) {
        auto it = changes.upper_bound(year);
        if (it == changes.begin())
            return {};
        if (!history)
            return std::prev(it)->second;
        std::vector<std::string> names;
        while (it != changes.begin()) {
            --it;
            if (names.empty() || names.back() != it->second)
                names.push_back(it->second);
        }
        std::string result = names.front();
        if (names.size() > 1) {
            result += " (";
            for (std::size_t i = 1; i < names.size(); ++i) {
                if (i > 1)
                    result += ", ";
                result += names[i];
            }
            result += ")";
        }
        return result;
    }
    std::string Full(int year, bool history) const {
        if (year < birth_)
            return "No person";
        const auto a = Name(first_, year, history), b = Name(last_, year, history);
        if (a.empty() && b.empty())
            return "Incognito";
        if (a.empty())
            return b + " with unknown first name";
        if (b.empty())
            return a + " with unknown last name";
        return a + " " + b;
    }

  public:
    void ChangeFirstName(int year, const std::string &name) {
        if (year >= birth_)
            first_[year] = name;
    }
    void ChangeLastName(int year, const std::string &name) {
        if (year >= birth_)
            last_[year] = name;
    }
    std::string GetFullName(int year) const { return Full(year, false); }
    std::string GetFullNameWithHistory(int year) const { return Full(year, true); }
};
namespace birth {
class Person : public white::Person {
  public:
    Person(const std::string &first, const std::string &last, int year) {
        birth_ = year;
        ChangeFirstName(year, first);
        ChangeLastName(year, last);
    }
};
} // namespace birth
} // namespace white
