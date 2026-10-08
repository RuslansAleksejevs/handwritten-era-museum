#include "responses.h"
namespace yellow::buses {
static void Join(std::ostream &out, const std::vector<std::string> &values) {
    bool first = true;
    for (const auto &s : values) {
        if (!first)
            out << ' ';
        first = false;
        out << s;
    }
}
std::ostream &operator<<(std::ostream &out, const BusesForStopResponse &r) {
    if (r.buses.empty())
        return out << "No stop";
    Join(out, r.buses);
    return out;
}
std::ostream &operator<<(std::ostream &out, const StopsForBusResponse &r) {
    if (r.stops.empty())
        return out << "No bus";
    bool first = true;
    for (const auto &[stop, buses] : r.stops) {
        if (!first)
            out << '\n';
        first = false;
        out << "Stop " << stop << ": ";
        if (buses.empty())
            out << "no interchange";
        else
            Join(out, buses);
    }
    return out;
}
std::ostream &operator<<(std::ostream &out, const AllBusesResponse &r) {
    if (r.buses.empty())
        return out << "No buses";
    bool first = true;
    for (const auto &[bus, stops] : r.buses) {
        if (!first)
            out << '\n';
        first = false;
        out << "Bus " << bus << ": ";
        Join(out, stops);
    }
    return out;
}
} // namespace yellow::buses
