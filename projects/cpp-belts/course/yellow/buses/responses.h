#pragma once
#include <map>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
namespace yellow::buses {
struct BusesForStopResponse {
    std::vector<std::string> buses;
};
struct StopsForBusResponse {
    std::vector<std::pair<std::string, std::vector<std::string>>> stops;
};
struct AllBusesResponse {
    std::map<std::string, std::vector<std::string>> buses;
};
std::ostream &operator<<(std::ostream &, const BusesForStopResponse &);
std::ostream &operator<<(std::ostream &, const StopsForBusResponse &);
std::ostream &operator<<(std::ostream &, const AllBusesResponse &);
} // namespace yellow::buses
