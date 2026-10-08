#pragma once
#include "date.hpp"
#include <map>
#include <set>
#include <sstream>
namespace white {
class Database {
    std::map<Date, std::set<std::string>> data_;

  public:
    void Add(const Date &date, const std::string &event) { data_[date].insert(event); }
    bool DeleteEvent(const Date &date, const std::string &event) {
        const auto it = data_.find(date);
        if (it == data_.end())
            return false;
        const bool removed = it->second.erase(event) > 0;
        if (it->second.empty())
            data_.erase(it);
        return removed;
    }
    std::size_t DeleteDate(const Date &date) {
        auto it = data_.find(date);
        if (it == data_.end())
            return 0;
        auto n = it->second.size();
        data_.erase(it);
        return n;
    }
    void Find(const Date &date, std::ostream &out) const {
        auto it = data_.find(date);
        if (it != data_.end())
            for (const auto &e : it->second)
                out << e << '\n';
    }
    void Print(std::ostream &out) const {
        for (const auto &[d, events] : data_)
            for (const auto &e : events)
                out << d << ' ' << e << '\n';
    }
};
inline void RunDatabase(std::istream &in, std::ostream &out) {
    Database db;
    std::string line;
    try {
        while (std::getline(in, line)) {
            std::istringstream row(line);
            std::string op, date, event;
            if (!(row >> op))
                continue;
            if (op == "Print") {
                db.Print(out);
                continue;
            }
            if (op != "Add" && op != "Del" && op != "Find")
                throw std::invalid_argument("Unknown command: " + op);
            row >> date;
            const auto d = ParseDate(date);
            if (op == "Add") {
                row >> event;
                db.Add(d, event);
            } else if (op == "Find")
                db.Find(d, out);
            else if (row >> event)
                out << (db.DeleteEvent(d, event) ? "Deleted successfully" : "Event not found")
                    << '\n';
            else
                out << "Deleted " << db.DeleteDate(d) << " events\n";
        }
    } catch (const std::invalid_argument &e) {
        out << e.what() << '\n';
    }
}
} // namespace white
