#pragma once
#include "date.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace yellow::events {
using Entry = std::pair<Date, std::string>;
std::ostream &operator<<(std::ostream &, const Entry &);
class Database {
    struct Day {
        std::vector<std::string> order;
        std::set<std::string> unique;
    };
    std::map<Date, Day> days_;

  public:
    void Add(const Date &, const std::string &);
    void Print(std::ostream &) const;
    Entry Last(const Date &) const;
    template <class Predicate> std::vector<Entry> FindIf(Predicate predicate) const {
        std::vector<Entry> found;
        for (const auto &[date, day] : days_)
            for (const auto &e : day.order)
                if (predicate(date, e))
                    found.emplace_back(date, e);
        return found;
    }
    template <class Predicate> int RemoveIf(Predicate predicate) {
        int removed = 0;
        for (auto it = days_.begin(); it != days_.end();) {
            auto &day = it->second;
            std::vector<std::string> kept;
            kept.reserve(day.order.size());
            for (const auto &e : day.order) {
                if (predicate(it->first, e)) {
                    day.unique.erase(e);
                    ++removed;
                } else
                    kept.push_back(e);
            }
            day.order.swap(kept);
            if (day.order.empty())
                it = days_.erase(it);
            else
                ++it;
        }
        return removed;
    }
};
void Run(std::istream &, std::ostream &);
} // namespace yellow::events
