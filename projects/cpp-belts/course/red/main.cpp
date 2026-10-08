#include "async_search.hpp"
#include "concurrency.hpp"
#include "services.hpp"
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
    using namespace museum::red;
    try {
        if (argc < 2)
            throw std::invalid_argument(
                "usage: red {express|reading|booking|sportsmen|learner|search} [documents]");
        const std::string mode = argv[1];
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);
        std::cout << std::setprecision(6);
        if (mode == "search") {
            if (argc != 3)
                throw std::invalid_argument("search needs documents file");
            std::ifstream docs(argv[2]);
            if (!docs)
                throw std::runtime_error("cannot read documents");
            museum::search::SearchServer server(docs);
            server.AddQueriesStream(std::cin, std::cout);
        } else if (mode == "learner") {
            Learner learner;
            for (std::string line; std::getline(std::cin, line);) {
                std::istringstream in(line);
                std::vector<std::string> words;
                for (std::string word; in >> word;)
                    words.push_back(word);
                std::cout << learner.Learn(words) << '\n';
            }
            std::cout << "=== known words ===\n";
            for (const auto &w : learner.KnownWords())
                std::cout << w << '\n';
        } else if (mode == "sportsmen") {
            std::size_t n = 0;
            std::cin >> n;
            std::vector<std::pair<int, int>> input(n);
            for (auto &[a, b] : input)
                std::cin >> a >> b;
            bool first = true;
            for (int id : Sportsmen(input)) {
                std::cout << (first ? "" : " ") << id;
                first = false;
            }
            std::cout << '\n';
        } else {
            std::size_t n = 0;
            if (!(std::cin >> n))
                throw std::invalid_argument("missing query count");
            Express express;
            ReadingManager reading;
            BookingManager booking;
            for (std::size_t i = 0; i < n; ++i) {
                std::string command;
                std::cin >> command;
                if (mode == "express") {
                    int a = 0, b = 0;
                    std::cin >> a >> b;
                    if (command == "ADD")
                        express.Add(a, b);
                    else if (command == "GO")
                        std::cout << express.Go(a, b) << '\n';
                    else
                        throw std::invalid_argument("express command");
                } else if (mode == "reading") {
                    int user = 0;
                    std::cin >> user;
                    if (command == "READ") {
                        int page = 0;
                        std::cin >> page;
                        reading.Read(user, page);
                    } else if (command == "CHEER")
                        std::cout << reading.Cheer(user) << '\n';
                    else
                        throw std::invalid_argument("reading command");
                } else if (mode == "booking") {
                    std::string hotel;
                    if (command == "BOOK") {
                        std::int64_t time = 0;
                        int client = 0, rooms = 0;
                        std::cin >> time >> hotel >> client >> rooms;
                        booking.Book(time, hotel, client, rooms);
                    } else {
                        std::cin >> hotel;
                        if (command == "CLIENTS")
                            std::cout << booking.Clients(hotel) << '\n';
                        else if (command == "ROOMS")
                            std::cout << booking.Rooms(hotel) << '\n';
                        else
                            throw std::invalid_argument("booking command");
                    }
                } else
                    throw std::invalid_argument("unknown exhibit");
                if (!std::cin)
                    throw std::invalid_argument("incomplete input");
            }
        }
        std::cout.flush();
        if (!std::cout)
            throw std::runtime_error("output failed");
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
