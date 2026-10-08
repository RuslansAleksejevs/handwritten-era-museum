#include "query.h"
#include <stdexcept>
namespace yellow::buses {
std::istream &operator>>(std::istream &in, Query &q) {
    std::string command;
    if (!(in >> command))
        return in;
    q = Query{};
    if (command == "NEW_BUS") {
        q.type = QueryType::NewBus;
        int n = 0;
        in >> q.bus >> n;
        if (n < 0)
            throw std::invalid_argument("negative stop count");
        q.stops.resize(n);
        for (auto &s : q.stops)
            in >> s;
    } else if (command == "BUSES_FOR_STOP") {
        q.type = QueryType::BusesForStop;
        in >> q.stop;
    } else if (command == "STOPS_FOR_BUS") {
        q.type = QueryType::StopsForBus;
        in >> q.bus;
    } else if (command == "ALL_BUSES")
        q.type = QueryType::AllBuses;
    else
        throw std::invalid_argument("unknown query");
    return in;
}
} // namespace yellow::buses
