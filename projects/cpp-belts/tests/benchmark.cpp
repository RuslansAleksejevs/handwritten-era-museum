#include "reference.hpp"
#include "../domains/domains.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {

using Clock = std::chrono::steady_clock;
using museum::search::Hit;

std::string Word(std::size_t value) {
    std::string word = "w";
    do { word += static_cast<char>('a' + value % 26); value /= 26; } while (value);
    return word;
}

std::uint64_t Digest(const std::vector<Hit>& hits) {
    std::uint64_t value = 0;
    for (const auto& hit : hits) value = value * 1099511628211ULL + hit.document_id * 31 + hit.count;
    return value;
}

struct Measurement {
    std::array<double, 3> milliseconds;
    std::uint64_t checksum;
};

template <class Function>
Measurement Measure(Function function) {
    Measurement result{};
    const auto expected = function();  // Untimed warm-up; keep work observable.
    for (auto& sample : result.milliseconds) {
        const auto start = Clock::now();
        result.checksum = function();
        sample = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        if (result.checksum != expected) throw std::runtime_error("unstable benchmark output");
    }
    return result;
}

void Print(const char* name, const Measurement& result) {
    auto sorted = result.milliseconds;
    std::sort(sorted.begin(), sorted.end());
    std::cout << '"' << name << "\": {\"samples_ms\": [";
    for (std::size_t i = 0; i < result.milliseconds.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << result.milliseconds[i];
    }
    std::cout << "], \"median_ms\": " << sorted[1]
              << ", \"checksum\": \"" << result.checksum << "\"}";
}

}  // namespace

int main() {
    try {
        std::mt19937 random(20261005);
        std::vector<std::vector<std::string>> documents(8000);
        std::ostringstream text;
        for (auto& document : documents) {
            for (int i = 0; i < 24; ++i) document.push_back(Word(random() % 1024));
            document.push_back("common");
            for (const auto& word : document) text << word << ' ';
            text << '\n';
        }
        std::istringstream input(text.str());
        const auto build_start = Clock::now();
        const museum::search::Index index(input);
        const auto build_ms = std::chrono::duration<double, std::milli>(Clock::now() - build_start).count();
        std::vector<std::string> rare_queries, common_queries;
        for (int i = 0; i < 200; ++i) {
            const auto words = Word(random() % 1024) + " " + Word(random() % 1024);
            rare_queries.push_back(words + " " + Word(random() % 1024));
            common_queries.push_back(words + " common");
        }
        museum::search::Workspace workspace;
        const auto verify = [&](const auto& queries) {
            for (const auto& query : queries) {
                if (index.Find(query, workspace) != reference::Search(documents, query)) {
                    throw std::runtime_error("benchmark answers differ from full scan");
                }
            }
        };
        verify(rare_queries);
        verify(common_queries);
        const auto indexed = [&](const auto& queries) {
            std::uint64_t sum = 0;
            for (const auto& query : queries) sum += Digest(index.Find(query, workspace));
            return sum;
        };
        const auto scanned = [&](const auto& queries) {
            std::uint64_t sum = 0;
            for (const auto& query : queries) sum += Digest(reference::Search(documents, query));
            return sum;
        };
        const auto rare_index = Measure([&] { return indexed(rare_queries); });
        const auto rare_scan = Measure([&] { return scanned(rare_queries); });
        const auto common_index = Measure([&] { return indexed(common_queries); });
        const auto common_scan = Measure([&] { return scanned(common_queries); });

        std::vector<std::string> banned, queries;
        for (int i = 0; i < 10000; ++i) banned.push_back(Word(i) + ".example");
        for (int i = 0; i < 10000; ++i) {
            queries.push_back("sub." + Word(random() % 10000) + (i % 2 ? ".example" : ".other"));
        }
        const museum::domains::Filter filter(banned);
        for (const auto& query : queries) {
            if (filter.IsBlocked(query) != reference::Blocked(banned, query)) {
                throw std::runtime_error("benchmark answers differ from suffix scan");
            }
        }
        const auto domains_index = Measure([&] {
            std::uint64_t count = 0;
            for (const auto& query : queries) count += filter.IsBlocked(query);
            return count;
        });
        const auto domains_scan = Measure([&] {
            std::uint64_t count = 0;
            for (const auto& query : queries) count += reference::Blocked(banned, query);
            return count;
        });

        std::cout << std::fixed << std::setprecision(3)
                  << "{\"seed\": 20261005, \"search_documents\": 8000, \"words_per_document\": 25, "
                     "\"search_queries_per_workload\": 200, \"domain_roots\": 10000, "
                     "\"domain_queries\": 10000, \"search_index_build_ms\": " << build_ms << ",\n";
        Print("search_rare_index", rare_index); std::cout << ",\n";
        Print("search_rare_scan", rare_scan); std::cout << ",\n";
        Print("search_common_index", common_index); std::cout << ",\n";
        Print("search_common_scan", common_scan); std::cout << ",\n";
        Print("domains_index", domains_index); std::cout << ",\n";
        Print("domains_scan", domains_scan); std::cout << "\n}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
