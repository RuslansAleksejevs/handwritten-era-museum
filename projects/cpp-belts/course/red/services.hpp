#pragma once
#include "algorithms.hpp"
#include <cstdlib>
#include <deque>
namespace museum::red {
class Express {
    std::map<int, std::set<int>> routes_;

  public:
    void Add(int a, int b) {
        routes_[a].insert(b);
        routes_[b].insert(a);
    }
    long long Go(int a, int b) const {
        long long distance = std::llabs(static_cast<long long>(a) - b);
        auto it = routes_.find(a);
        if (it == routes_.end())
            return distance;
        auto after = it->second.lower_bound(b);
        if (after != it->second.end())
            distance = std::min(distance, std::llabs(static_cast<long long>(*after) - b));
        if (after != it->second.begin())
            distance =
                std::min(distance, std::llabs(static_cast<long long>(*std::prev(after)) - b));
        return distance;
    }
};
class ReadingManager {
    std::vector<int> pages_ = std::vector<int>(100001);
    std::array<int, 1002> tree_{};
    int readers_ = 0;
    void Add(int page, int delta) {
        for (auto i = page; i < static_cast<int>(tree_.size()); i += i & -i)
            tree_[i] += delta;
    }

  public:
    void Read(int user, int page) {
        if (user < 1 || user > 100000 || page < 1 || page > 1000)
            throw std::out_of_range("reader or page");
        auto &old = pages_[user];
        if (old)
            Add(old, -1);
        else
            ++readers_;
        old = page;
        Add(page, 1);
    }
    double Cheer(int user) const {
        if (user < 1 || user > 100000 || !pages_[user])
            return 0;
        if (readers_ == 1)
            return 1;
        int behind = 0;
        for (int i = pages_[user] - 1; i > 0; i -= i & -i)
            behind += tree_[i];
        return static_cast<double>(behind) / (readers_ - 1);
    }
};
class BookingManager {
    struct Event {
        std::int64_t time;
        std::string hotel;
        int client, rooms;
    };
    struct Hotel {
        long long rooms = 0;
        std::unordered_map<int, int> clients;
    };
    std::deque<Event> history_;
    std::unordered_map<std::string, Hotel> hotels_;

  public:
    void Book(std::int64_t time, std::string hotel, int client, int rooms) {
        history_.push_back({time, hotel, client, rooms});
        auto &current = hotels_[hotel];
        current.rooms += rooms;
        ++current.clients[client];
        while (!history_.empty() && history_.front().time <= time - 86400) {
            const auto &old = history_.front();
            auto &state = hotels_.at(old.hotel);
            state.rooms -= old.rooms;
            auto it = state.clients.find(old.client);
            if (--it->second == 0)
                state.clients.erase(it);
            history_.pop_front();
        }
    }
    std::size_t Clients(const std::string &hotel) const {
        auto it = hotels_.find(hotel);
        return it == hotels_.end() ? 0 : it->second.clients.size();
    }
    long long Rooms(const std::string &hotel) const {
        auto it = hotels_.find(hotel);
        return it == hotels_.end() ? 0 : it->second.rooms;
    }
};
inline std::vector<int> Sportsmen(const std::vector<std::pair<int, int>> &arrivals) {
    std::list<int> line;
    std::unordered_map<int, std::list<int>::iterator> positions;
    for (auto [id, before] : arrivals) {
        auto it = positions.find(before);
        positions[id] = line.insert(it == positions.end() ? line.end() : it->second, id);
    }
    return {line.begin(), line.end()};
}
} // namespace museum::red
