#pragma once
#include <istream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace white {
class BusManager {
    std::map<std::string, std::vector<std::string>> buses_, stops_;

  public:
    void AddBus(const std::string &bus, const std::vector<std::string> &stops) {
        if (buses_.count(bus))
            throw std::invalid_argument("duplicate bus");
        buses_[bus] = stops;
        for (const auto &stop : stops)
            stops_[stop].push_back(bus);
    }
    const std::map<std::string, std::vector<std::string>> &All() const { return buses_; }
    const std::vector<std::string> &Buses(const std::string &stop) const {
        static const std::vector<std::string> empty;
        const auto i = stops_.find(stop);
        return i == stops_.end() ? empty : i->second;
    }
    const std::vector<std::string> &Stops(const std::string &bus) const {
        static const std::vector<std::string> empty;
        const auto i = buses_.find(bus);
        return i == buses_.end() ? empty : i->second;
    }
};
template <class Range> void PrintWords(const Range &values, std::ostream &out) {
    bool first = true;
    for (const auto &x : values) {
        if (!first)
            out << ' ';
        first = false;
        out << x;
    }
}
inline void RunBuses(std::istream &in, std::ostream &out) {
    int count = 0;
    in >> count;
    BusManager buses;
    for (int i = 0; i < count; ++i) {
        std::string op, name;
        in >> op;
        if (op == "NEW_BUS") {
            int n = 0;
            in >> name >> n;
            std::vector<std::string> stops(n);
            for (auto &s : stops)
                in >> s;
            buses.AddBus(name, stops);
        } else if (op == "BUSES_FOR_STOP") {
            in >> name;
            const auto &b = buses.Buses(name);
            if (b.empty())
                out << "No stop";
            else
                PrintWords(b, out);
            out << '\n';
        } else if (op == "STOPS_FOR_BUS") {
            in >> name;
            const auto &stops = buses.Stops(name);
            if (stops.empty())
                out << "No bus\n";
            for (const auto &stop : stops) {
                out << "Stop " << stop << ": ";
                std::vector<std::string> other;
                for (const auto &b : buses.Buses(stop))
                    if (b != name)
                        other.push_back(b);
                if (other.empty())
                    out << "no interchange";
                else
                    PrintWords(other, out);
                out << '\n';
            }
        } else if (op == "ALL_BUSES") {
            if (buses.All().empty())
                out << "No buses\n";
            for (const auto &[b, stops] : buses.All()) {
                out << "Bus " << b << ": ";
                PrintWords(stops, out);
                out << '\n';
            }
        } else
            throw std::invalid_argument("unknown bus query");
    }
}
} // namespace white
