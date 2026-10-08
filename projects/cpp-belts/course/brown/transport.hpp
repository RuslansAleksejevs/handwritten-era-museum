#pragma once
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
namespace museum::brown {
struct Stop {
    std::string name;
    double latitude = 0, longitude = 0;
    std::map<std::string, int> road_distances;
};
struct Bus {
    std::string name;
    std::vector<std::string> stops;
    bool is_roundtrip = false;
};
struct RouteStats {
    std::size_t stop_count = 0, unique_stop_count = 0;
    double route_length = 0, geographic_length = 0, curvature = 0;
};
class Transport {
    std::map<std::string, Stop> stops_;
    std::map<std::string, Bus> buses_;
    std::map<std::string, std::set<std::string>> stop_buses_;

  public:
    void AddStop(Stop stop) {
        if (!std::isfinite(stop.latitude) || !std::isfinite(stop.longitude) ||
            std::abs(stop.latitude) > 90 || std::abs(stop.longitude) > 180)
            throw std::invalid_argument("invalid geographic coordinates");
        for (const auto &pair : stop.road_distances)
            if (pair.second < 0)
                throw std::invalid_argument("negative road distance");
        stops_.insert_or_assign(stop.name, std::move(stop));
    }
    void AddBus(Bus bus) {
        if (auto it = buses_.find(bus.name); it != buses_.end())
            for (const auto &s : it->second.stops)
                stop_buses_[s].erase(bus.name);
        for (const auto &s : bus.stops)
            stop_buses_[s].insert(bus.name);
        buses_.insert_or_assign(bus.name, std::move(bus));
    }
    const auto &Stops() const { return stops_; }
    const auto &Buses() const { return buses_; }
    const Stop &GetStop(const std::string &name) const { return stops_.at(name); }
    static double GeographicDistance(const Stop &a, const Stop &b) {
        if (a.latitude == b.latitude && a.longitude == b.longitude)
            return 0;
        constexpr double rad = 3.1415926535 / 180;
        const double x = a.latitude * rad, y = b.latitude * rad;
        return 6371000 * std::acos(std::clamp(std::sin(x) * std::sin(y) +
                                                  std::cos(x) * std::cos(y) *
                                                      std::cos((a.longitude - b.longitude) * rad),
                                              -1.0, 1.0));
    }
    int RoadDistance(const std::string &from, const std::string &to) const {
        const auto &a = stops_.at(from);
        if (auto it = a.road_distances.find(to); it != a.road_distances.end())
            return it->second;
        if (from == to)
            return 0;
        return stops_.at(to).road_distances.at(from);
    }
    static std::vector<std::string> Traversal(const Bus &bus) {
        auto result = bus.stops;
        if (!bus.is_roundtrip && result.size() > 1)
            for (std::size_t i = bus.stops.size() - 1; i > 0; --i)
                result.push_back(bus.stops[i - 1]);
        return result;
    }
    std::optional<RouteStats> BusStats(const std::string &name, bool roads = true) const {
        auto it = buses_.find(name);
        if (it == buses_.end())
            return std::nullopt;
        const auto sequence = Traversal(it->second);
        RouteStats out;
        out.stop_count = sequence.size();
        out.unique_stop_count = std::set<std::string>(sequence.begin(), sequence.end()).size();
        for (std::size_t i = 1; i < sequence.size(); ++i) {
            auto g = GeographicDistance(GetStop(sequence[i - 1]), GetStop(sequence[i]));
            out.geographic_length += g;
            out.route_length += roads ? RoadDistance(sequence[i - 1], sequence[i]) : g;
        }
        out.curvature = out.geographic_length ? out.route_length / out.geographic_length : 0;
        return out;
    }
    std::optional<std::set<std::string>> StopBuses(const std::string &name) const {
        if (!stops_.count(name))
            return std::nullopt;
        auto it = stop_buses_.find(name);
        return it == stop_buses_.end() ? std::set<std::string>{} : it->second;
    }
    static Transport FromJson(const json::Node &root) {
        Transport result;
        for (const auto &request : root.At("base_requests").AsArray())
            if (request.At("type").AsString() == "Stop") {
                Stop stop{request.At("name").AsString(),
                          request.At("latitude").AsNumber(),
                          request.At("longitude").AsNumber(),
                          {}};
                if (request.Contains("road_distances"))
                    for (const auto &[name, distance] : request.At("road_distances").AsObject())
                        stop.road_distances[name] = distance.AsInt();
                result.AddStop(std::move(stop));
            }
        for (const auto &request : root.At("base_requests").AsArray())
            if (request.At("type").AsString() == "Bus") {
                Bus bus{request.At("name").AsString(), {}, request.At("is_roundtrip").AsBool()};
                for (const auto &s : request.At("stops").AsArray())
                    bus.stops.push_back(s.AsString());
                result.AddBus(std::move(bus));
            }
        return result;
    }
    json::Node Answer(const json::Node &request) const {
        json::Object out{{"request_id", request.At("id")}};
        const auto &name = request.At("name").AsString();
        if (request.At("type").AsString() == "Bus") {
            auto stats = BusStats(name);
            if (stats) {
                out["stop_count"] = stats->stop_count;
                out["unique_stop_count"] = stats->unique_stop_count;
                out["route_length"] = stats->route_length;
                out["curvature"] = stats->curvature;
            } else
                out["error_message"] = "not found";
        } else if (request.At("type").AsString() == "Stop") {
            auto buses = StopBuses(name);
            if (buses) {
                json::Array names;
                for (const auto &bus : *buses)
                    names.emplace_back(bus);
                out["buses"] = std::move(names);
            } else
                out["error_message"] = "not found";
        } else
            throw std::invalid_argument("unsupported transport request");
        return out;
    }
};
} // namespace museum::brown
