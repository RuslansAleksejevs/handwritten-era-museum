#pragma once
#include <istream>
#include <string>
#include <vector>
namespace yellow::buses {
enum class QueryType { NewBus, BusesForStop, StopsForBus, AllBuses };
struct Query {
    QueryType type = QueryType::AllBuses;
    std::string bus, stop;
    std::vector<std::string> stops;
};
std::istream &operator>>(std::istream &, Query &);
} // namespace yellow::buses
