#include "reference.hpp"
#include "../domains/domains.hpp"

#include <atomic>
#include <future>
#include <iostream>
#include <random>
#include <stdexcept>
#include <streambuf>
#include <thread>

namespace {

using museum::search::Hit;
using museum::search::Index;
using museum::search::SearchServer;
using museum::search::Workspace;
using museum::domains::Filter;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <class Function>
void CheckThrows(Function function) {
    bool threw = false;
    try { function(); } catch (const std::exception&) { threw = true; }
    Check(threw, "expected an exception");
}

void ExamplesAndBoundaries() {
    std::istringstream documents("london is the capital of great britain\n"
                                 "moscow is the capital of the russian federation\n"
                                 "paris is the capital of france\n");
    const SearchServer server(documents);
    std::istringstream queries("the best capital\n  the the   \nunknown\n\n");
    std::ostringstream output;
    server.AddQueriesStream(queries, output);
    Check(output.str() ==
        "the best capital: {docid: 1, hitcount: 3} {docid: 0, hitcount: 2} {docid: 2, hitcount: 2}\n"
        "  the the   : {docid: 1, hitcount: 4} {docid: 0, hitcount: 2} {docid: 2, hitcount: 2}\n"
        "unknown:\n:\n", "query format, multiplicity or ranking");

    Workspace workspace;
    Check(server.Find("cap", workspace).empty(), "matching must use complete words");
    std::istringstream bad_queries("anything");
    bad_queries.setstate(std::ios::badbit);
    CheckThrows([&] { server.AddQueriesStream(bad_queries, output); });
    std::istringstream valid_queries("the");
    std::ostringstream bad_output;
    bad_output.setstate(std::ios::badbit);
    CheckThrows([&] { server.AddQueriesStream(valid_queries, bad_output); });

    std::string corpus;
    for (int i = 0; i < 50000; ++i) corpus += "x\n";
    std::istringstream large(corpus);
    const Index index(large);
    Check(index.DocumentCount() == 50000, "document limit");
    Check(index.Find("x", workspace) == std::vector<Hit>{{0,1},{1,1},{2,1},{3,1},{4,1}},
          "top five ties must use ascending document ID");
    std::string repeated;
    for (int i = 0; i < 1000; ++i) repeated += "word ";
    std::istringstream dense(repeated);
    const Index dense_index(dense);
    Check(dense_index.Find("word word word word word word word word word word", workspace)
          == std::vector<Hit>{{0,10000}}, "maximum assignment hit count");
}

void ReplacementsAndLifetimes() {
    SearchServer server;
    Workspace workspace;
    Check(server.Find("a", workspace).empty(), "default server");
    {
        std::istringstream input("a a\n\na");
        server.UpdateDocumentBase(input);
    }  // All terms must survive destruction of the input stream.
    Check(server.Find("a a", workspace) == std::vector<Hit>{{0,4},{2,2}}, "owned words");
    std::istringstream replacement("a\na a\na a a");
    server.UpdateDocumentBase(replacement);
    Check(server.Find("a", workspace) == std::vector<Hit>{{2,3},{1,2},{0,1}},
          "same-sized replacement must clear previous scores");
    std::istringstream failed("b");
    failed.setstate(std::ios::badbit);
    CheckThrows([&] { server.UpdateDocumentBase(failed); });
    Check(server.Find("a", workspace) == std::vector<Hit>{{2,3},{1,2},{0,1}},
          "failed replacement must retain the old index");
    std::istringstream empty;
    server.UpdateDocumentBase(empty);
    Check(server.Find("a", workspace).empty(), "empty replacement");
}

void RandomSearch() {
    std::mt19937 random(20261005);
    const std::vector<std::string> vocabulary{"a", "aa", "b", "bb", "c", "ccc", "d"};
    Workspace workspace;
    for (int trial = 0; trial < 200; ++trial) {
        std::vector<std::vector<std::string>> documents(random() % 41);
        std::ostringstream text;
        for (auto& document : documents) {
            const auto length = random() % 15;
            for (unsigned i = 0; i < length; ++i) {
                document.push_back(vocabulary[random() % vocabulary.size()]);
                text << document.back() << std::string(1 + random() % 3, ' ');
            }
            text << '\n';
        }
        std::istringstream stream(text.str());
        const Index index(stream);
        for (int q = 0; q < 120; ++q) {
            std::string query;
            const auto count = random() % 11;
            for (unsigned i = 0; i < count; ++i) {
                query += (random() % 5 == 0 ? "missing" : vocabulary[random() % vocabulary.size()]);
                query += std::string(1 + random() % 3, ' ');
            }
            Check(index.Find(query, workspace) == reference::Search(documents, query),
                  "random search disagrees with full-scan oracle");
        }
    }
}

class GatedInput : public std::streambuf {
public:
    GatedInput(std::promise<void>& entered, std::shared_future<void> release)
        : entered_(entered), release_(std::move(release)) {}
private:
    int_type underflow() override {
        if (read_) return traits_type::eof();
        read_ = true;
        entered_.set_value();
        release_.wait();
        setg(text_.data(), text_.data(), text_.data() + text_.size());
        return traits_type::to_int_type(*gptr());
    }
    std::promise<void>& entered_;
    std::shared_future<void> release_;
    std::string text_ = "new new\n";
    bool read_ = false;
};

void ConcurrentReplacement() {
    std::istringstream original("old\n");
    SearchServer server(original);
    std::promise<void> entered, release;
    GatedInput buffer(entered, release.get_future().share());
    std::istream input(&buffer);
    auto writer = std::async(std::launch::async, [&] { server.UpdateDocumentBase(input); });
    entered.get_future().wait();
    Workspace workspace;
    const auto during = server.Find("old new", workspace);
    release.set_value();
    writer.get();
    Check(during == std::vector<Hit>{{0,1}}, "readers must progress while input is blocked");
    Check(server.Find("old new", workspace) == std::vector<Hit>{{0,2}}, "publish new index");

    std::istringstream first("other\nx x\n");
    server.UpdateDocumentBase(first);
    std::promise<void> start;
    const auto ready = start.get_future().share();
    std::atomic<unsigned> finished{0};
    std::vector<std::future<void>> readers;
    for (int i = 0; i < 4; ++i) {
        readers.push_back(std::async(std::launch::async, [&] {
            ready.wait();
            Workspace own_workspace;
            for (int q = 0; q < 10000; ++q) {
                const auto result = server.Find("x", own_workspace);
                Check(result == std::vector<Hit>{{1,2}} || result == std::vector<Hit>{{0,3}},
                      "a query observed a mixture of index versions");
            }
            ++finished;
        }));
    }
    auto updater = std::async(std::launch::async, [&] {
        ready.wait();
        // Bound the writer even if a reader reports a test failure.
        for (unsigned i = 0; i < 1000 && finished != 4; ++i) {
            std::istringstream next(i % 2 ? "other\nx x\n" : "x x x\n");
            server.UpdateDocumentBase(next);
        }
    });
    start.set_value();
    for (auto& reader : readers) reader.get();
    updater.get();
}

std::string RandomDomain(std::mt19937& random) {
    const std::vector<std::string> labels{"a", "aa", "ab", "b", "ba", "com", "ru"};
    std::string result;
    const auto count = 1 + random() % 5;
    for (unsigned i = 0; i < count; ++i) {
        if (i) result += '.';
        result += labels[random() % labels.size()];
    }
    return result;
}

void Domains() {
    const Filter filter({"ya.ru", "maps.me", "m.ya.ru", "com", "ya.ru"});
    Check(filter.RootCount() == 3, "remove duplicate and covered roots");
    for (const auto* domain : {"ya.ru", "ya.com", "m.maps.me", "moscow.m.ya.ru", "maps.com"}) {
        Check(filter.IsBlocked(domain), "assignment positive example");
    }
    for (const auto* domain : {"maps.ru", "ya.ya", "notya.ru", "ya.rus", "ru"}) {
        Check(!filter.IsBlocked(domain), "label boundary or direction");
    }
    const Filter empty({});
    Check(!empty.IsBlocked("any.domain"), "empty ban list");
    for (const auto* invalid : {"", ".a", "a.", "a..b", "A.ru", "a1.ru", "a b"}) {
        CheckThrows([&] { Filter bad({invalid}); });
        CheckThrows([&] { filter.IsBlocked(invalid); });
    }
    std::mt19937 random(7052026);
    for (int trial = 0; trial < 400; ++trial) {
        std::vector<std::string> banned;
        const auto count = random() % 61;
        for (unsigned i = 0; i < count; ++i) banned.push_back(RandomDomain(random));
        const Filter candidate(banned);
        for (int i = 0; i < 250; ++i) {
            const auto query = RandomDomain(random);
            Check(candidate.IsBlocked(query) == reference::Blocked(banned, query),
                  "domain filter disagrees with suffix oracle");
        }
    }
}

}  // namespace

int main() {
    try {
        ExamplesAndBoundaries();
        ReplacementsAndLifetimes();
        RandomSearch();
        ConcurrentReplacement();
        Domains();
        std::cout << "PASS: 24000 search oracle cases, 100000 domain oracle cases, "
                     "40000 concurrent queries, input failures and boundary cases\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
