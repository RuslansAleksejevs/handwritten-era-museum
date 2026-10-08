#include "bus_manager.h"
#include "query.h"
namespace yellow::buses {
void BusManager::AddBus(const std::string &name, const std::vector<std::string> &stops) {
    storage_.AddBus(name, stops);
}
BusesForStopResponse BusManager::GetBusesForStop(const std::string &stop) const {
    return {storage_.Buses(stop)};
}
StopsForBusResponse BusManager::GetStopsForBus(const std::string &bus) const {
    StopsForBusResponse r;
    for (const auto &s : storage_.Stops(bus)) {
        std::vector<std::string> other;
        for (const auto &b : storage_.Buses(s))
            if (b != bus)
                other.push_back(b);
        r.stops.emplace_back(s, std::move(other));
    }
    return r;
}
AllBusesResponse BusManager::GetAllBuses() const { return {storage_.All()}; }
void Run(std::istream &in, std::ostream &out) {
    int count = 0;
    in >> count;
    BusManager manager;
    for (int i = 0; i < count; ++i) {
        Query q;
        in >> q;
        switch (q.type) {
        case QueryType::NewBus:
            manager.AddBus(q.bus, q.stops);
            break;
        case QueryType::BusesForStop:
            out << manager.GetBusesForStop(q.stop) << '\n';
            break;
        case QueryType::StopsForBus:
            out << manager.GetStopsForBus(q.bus) << '\n';
            break;
        case QueryType::AllBuses:
            out << manager.GetAllBuses() << '\n';
            break;
        }
    }
}
} // namespace yellow::buses
