#include "budget.hpp"
#include "formats.hpp"
#include "services.hpp"
#include "transport.hpp"
#include <iostream>
namespace {
std::string Trim(std::string s) {
    const auto begin = s.find_first_not_of(' ');
    if (begin == s.npos)
        return {};
    return s.substr(begin, s.find_last_not_of(' ') - begin + 1);
}
void TransportText(bool roads) {
    using namespace museum::brown;
    Transport database;
    std::size_t count = 0;
    std::cin >> count;
    std::string line;
    std::getline(std::cin, line);
    for (std::size_t i = 0; i < count; ++i) {
        std::getline(std::cin, line);
        const auto colon = line.find(':');
        if (colon == line.npos)
            throw std::invalid_argument("transport definition needs colon");
        const auto name = line.substr(line.find(' ') + 1, colon - line.find(' ') - 1);
        const auto body = Trim(line.substr(colon + 1));
        if (line.rfind("Stop ", 0) == 0) {
            std::istringstream in(body);
            std::string lat, lon;
            std::getline(in, lat, ',');
            std::getline(in, lon, ',');
            Stop stop{name, std::stod(lat), std::stod(lon), {}};
            for (std::string road; std::getline(in, road, ',');) {
                road = Trim(road);
                const auto split = road.find("m to ");
                if (split == road.npos)
                    throw std::invalid_argument("invalid road distance");
                stop.road_distances[road.substr(split + 5)] = std::stoi(road.substr(0, split));
            }
            database.AddStop(std::move(stop));
        } else if (line.rfind("Bus ", 0) == 0) {
            const bool round = body.find(" > ") != body.npos;
            const std::string separator = round ? " > " : " - ";
            Bus bus{name, {}, round};
            std::size_t begin = 0;
            for (;;) {
                auto end = body.find(separator, begin);
                bus.stops.push_back(body.substr(begin, end - begin));
                if (end == body.npos)
                    break;
                begin = end + separator.size();
            }
            database.AddBus(std::move(bus));
        } else
            throw std::invalid_argument("unknown transport definition");
    }
    std::cin >> count;
    std::getline(std::cin, line);
    std::cout << std::setprecision(6);
    for (std::size_t i = 0; i < count; ++i) {
        std::getline(std::cin, line);
        const auto split = line.find(' ');
        const auto type = line.substr(0, split), name = line.substr(split + 1);
        std::cout << type << ' ' << name << ": ";
        if (type == "Bus") {
            auto stats = database.BusStats(name, roads);
            if (!stats)
                std::cout << "not found";
            else {
                std::cout << stats->stop_count << " stops on route, " << stats->unique_stop_count
                          << " unique stops, " << stats->route_length << " route length";
                if (roads)
                    std::cout << ", " << stats->curvature << " curvature";
            }
        } else if (type == "Stop") {
            auto buses = database.StopBuses(name);
            if (!buses)
                std::cout << "not found";
            else if (buses->empty())
                std::cout << "no buses";
            else {
                std::cout << "buses";
                for (const auto &bus : *buses)
                    std::cout << ' ' << bus;
            }
        } else
            throw std::invalid_argument("unknown transport query");
        std::cout << '\n';
    }
}
} // namespace
int main(int argc, char **argv) {
    using namespace museum::brown;
    try {
        if (argc != 2)
            throw std::invalid_argument("usage: brown "
                                        "{budget-home|budget|transport-ab|transport-c|transport-"
                                        "json|demographics|persons|domains|xml-json|json-xml}");
        const std::string mode = argv[1];
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);
        std::cout << std::setprecision(17);
        if (mode == "budget" || mode == "budget-home") {
            Budget budget;
            std::size_t count = 0;
            std::cin >> count;
            for (std::size_t i = 0; i < count; ++i) {
                std::string command, from, to;
                std::cin >> command >> from >> to;
                if (command == "ComputeIncome")
                    std::cout << budget.ComputeIncome(from, to) << '\n';
                else if (command == "PayTax") {
                    double percent = 13;
                    if (mode == "budget")
                        std::cin >> percent;
                    budget.PayTax(from, to, percent);
                } else {
                    double amount = 0;
                    std::cin >> amount;
                    if (command == "Earn")
                        budget.Earn(from, to, amount);
                    else if (command == "Spend")
                        budget.Spend(from, to, amount);
                    else
                        throw std::invalid_argument("unknown budget command");
                }
                if (!std::cin)
                    throw std::invalid_argument("incomplete budget command");
            }
        } else if (mode == "transport-ab" || mode == "transport-c")
            TransportText(mode == "transport-c");
        else if (mode == "transport-json") {
            const auto root = json::Load(std::cin);
            const auto database = Transport::FromJson(root);
            json::Array answers;
            for (const auto &query : root.At("stat_requests").AsArray())
                answers.push_back(database.Answer(query));
            json::Serialize(answers, std::cout);
            std::cout << '\n';
        } else if (mode == "demographics") {
            std::size_t n = 0;
            std::cin >> n;
            std::vector<Citizen> people(n);
            for (auto &p : people)
                std::cin >> p.name >> p.age >> p.income >> p.gender;
            const Demographics data(people);
            for (std::string command; std::cin >> command;) {
                if (command == "AGE") {
                    int age = 0;
                    std::cin >> age;
                    std::cout << "There are " << data.Adults(age)
                              << " adult people for maturity age " << age << '\n';
                } else if (command == "WEALTHY") {
                    std::size_t top = 0;
                    std::cin >> top;
                    std::cout << "Top-" << top << " people have total income " << data.Wealthy(top)
                              << '\n';
                } else if (command == "POPULAR_NAME") {
                    char gender = 0;
                    std::cin >> gender;
                    auto name = data.Popular(gender);
                    if (name)
                        std::cout << "Most popular name among people of gender " << gender << " is "
                                  << *name << '\n';
                    else
                        std::cout << "No people of gender " << gender << '\n';
                } else
                    throw std::invalid_argument("demographics command");
            }
        } else if (mode == "persons") {
            std::size_t n = 0;
            std::cin >> n;
            std::vector<persons::Person> people;
            for (std::size_t i = 0; i < n; ++i) {
                int age = 0, gender = 0, employed = 0;
                std::cin >> age >> gender >> employed;
                people.push_back({age, static_cast<persons::Gender>(gender), employed == 1});
            }
            persons::PrintStats(persons::ComputeStats(people));
        } else if (mode == "domains") {
            std::size_t n = 0;
            std::cin >> n;
            std::vector<std::string> banned(n);
            for (auto &value : banned)
                std::cin >> value;
            museum::domains::Filter filter(std::move(banned));
            std::cin >> n;
            for (std::size_t i = 0; i < n; ++i) {
                std::string query;
                std::cin >> query;
                std::cout << (filter.IsBlocked(query) ? "Bad" : "Good") << '\n';
            }
        } else if (mode == "xml-json") {
            auto doc = Xml::Load(std::cin);
            json::Serialize(XmlToJson(doc), std::cout);
            std::cout << '\n';
        } else if (mode == "json-xml") {
            Xml::Print(JsonToXml(json::Load(std::cin), "expenses"), std::cout);
        } else
            throw std::invalid_argument("unknown exhibit");
        std::cout.flush();
        if (!std::cout)
            throw std::runtime_error("output failed");
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
