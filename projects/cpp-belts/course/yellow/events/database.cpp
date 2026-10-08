#include "database.h"
#include "condition_parser.h"
#include <sstream>
namespace yellow::events {
std::ostream &operator<<(std::ostream &out, const Entry &e) {
    return out << e.first << ' ' << e.second;
}
void Database::Add(const Date &d, const std::string &e) {
    auto &day = days_[d];
    if (day.unique.insert(e).second)
        day.order.push_back(e);
}
void Database::Print(std::ostream &out) const {
    for (const auto &[d, day] : days_)
        for (const auto &e : day.order)
            out << d << ' ' << e << '\n';
}
Entry Database::Last(const Date &d) const {
    auto it = days_.upper_bound(d);
    if (it == days_.begin())
        throw std::invalid_argument("No entries");
    --it;
    return {it->first, it->second.order.back()};
}
void Run(std::istream &in, std::ostream &out) {
    Database db;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream row(line);
        std::string command;
        if (!(row >> command))
            continue;
        if (command == "Add") {
            const auto date = ParseDate(row);
            std::string event;
            std::getline(row, event);
            auto first = event.find_first_not_of(' ');
            event = first == event.npos ? "" : event.substr(first);
            db.Add(date, event);
        } else if (command == "Print")
            db.Print(out);
        else if (command == "Last") {
            const auto date = ParseDate(row);
            try {
                out << db.Last(date) << '\n';
            } catch (const std::invalid_argument &) {
                out << "No entries\n";
            }
        } else if (command == "Find" || command == "Del") {
            auto condition = ParseCondition(row);
            auto predicate = [&](const Date &d, const std::string &e) {
                return condition->Evaluate(d, e);
            };
            if (command == "Del")
                out << "Removed " << db.RemoveIf(predicate) << " entries\n";
            else {
                auto found = db.FindIf(predicate);
                for (const auto &e : found)
                    out << e << '\n';
                out << "Found " << found.size() << " entries\n";
            }
        } else
            throw std::logic_error("Unknown command: " + command);
    }
}
} // namespace yellow::events
