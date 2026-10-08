#pragma once
#include "../../white/buses.hpp"
#include "responses.h"
namespace yellow::buses {
class BusManager {
    white::BusManager storage_;

  public:
    void AddBus(const std::string &, const std::vector<std::string> &);
    BusesForStopResponse GetBusesForStop(const std::string &) const;
    StopsForBusResponse GetStopsForBus(const std::string &) const;
    AllBusesResponse GetAllBuses() const;
};
void Run(std::istream &, std::ostream &);
} // namespace yellow::buses
