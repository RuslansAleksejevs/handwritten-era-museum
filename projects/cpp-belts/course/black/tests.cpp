#include "foundations.hpp"
#include "json_printer.hpp"
#include "phone_book.hpp"
#include "spreadsheet.hpp"
#include "transport.hpp"
#include "yellow_pages.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <sstream>

using namespace museum::black;
namespace ss = museum::black::spreadsheet;

template <class F> void Throws(F f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception &) {
        caught = true;
    }
    assert(caught);
}
struct Tracked {
    static inline int alive = 0, copies_left = -1;
    int value;
    explicit Tracked(int v = 0) : value(v) { ++alive; }
    Tracked(const Tracked &x) : value(x.value) {
        if (copies_left == 0)
            throw std::runtime_error("copy failure");
        if (copies_left > 0)
            --copies_left;
        ++alive;
    }
    Tracked(Tracked &&x) noexcept(false) : value(x.value) {
        ++alive;
        x.value = -1;
    }
    Tracked &operator=(const Tracked &x) {
        value = x.value;
        return *this;
    }
    Tracked &operator=(Tracked &&x) noexcept {
        value = x.value;
        x.value = -1;
        return *this;
    }
    ~Tracked() { --alive; }
};
struct ConstructOnly {
    const int value;
    explicit ConstructOnly(int x) : value(x) {}
};
void Foundations() {
    using I = std::int64_t;
    const I lo = std::numeric_limits<I>::min(), hi = std::numeric_limits<I>::max();
    assert(CheckedSum(lo, hi) == -1 && CheckedSum(lo, 0) == lo && CheckedSum(hi, 0) == hi);
    Throws([&] { CheckedSum(lo, -1); });
    Throws([&] { CheckedSum(hi, 1); });
    assert(!EqualsToOneOf(5) && EqualsToOneOf(5, 1, 5, 8) && !EqualsToOneOf(5, 1, 2));
    std::string sequence;
    ApplyToMany([&](int x) { sequence += std::to_string(x); }, 1, 2, 3);
    assert(sequence == "123");
    const ConstructOnly fixed(7);
    Optional<ConstructOnly> constructed(fixed), copied(constructed), moved(std::move(copied));
    assert(moved->value == 7);
    Vector<ConstructOnly> append_only;
    append_only.EmplaceBack(1);
    append_only.PushBack(fixed);
    append_only.Reserve(10);
    append_only.EmplaceBack(3);
    assert(append_only.Size() == 3 && append_only[1].value == 7);
    {
        Optional<Tracked> a;
        assert(Tracked::alive == 0 && !a);
        Throws([&] { a.Value(); });
        a.Emplace(7);
        Optional<Tracked> b(a);
        assert(b->value == 7 && Tracked::alive == 2);
        a.Reset();
        b = a;
        assert(Tracked::alive == 0);
        b.Emplace(8);
        auto *same = &b;
        b = std::move(*same);
        assert(b->value == 8);
        Tracked::copies_left = 0;
        Throws([&] { a = *b; });
        assert(!a);
        Tracked::copies_left = -1;
    }
    assert(Tracked::alive == 0);
    {
        Vector<Tracked> v;
        v.EmplaceBack(10);
        v.EmplaceBack(20);
        v.EmplaceBack(30);
        Tracked::copies_left = 1;
        Throws([&] { v.Reserve(20); });
        assert(v.Size() == 3 && v[0].value == 10 && v[1].value == 20 && v[2].value == 30 &&
               Tracked::alive == 3);
        Tracked::copies_left = -1;
        v.Reserve(10);
        v.Insert(v.begin() + 1, v[2]);
        assert(v.Size() == 4 && v[1].value == 30 && v[2].value == 20 && v[3].value == 30);
        v.Erase(v.begin() + 2);
        v.Resize(7);
        v.Resize(1);
        Vector<Tracked> copy(v);
        auto moved = std::move(copy);
        assert(moved.Size() == 1 && copy.Size() == 0 && moved[0].value == 10);
    }
    assert(Tracked::alive == 0);
    std::mt19937 rng(1729);
    Vector<int> custom;
    std::vector<int> oracle;
    for (int i = 0; i < 15000; ++i) {
        const auto operation = rng() % 5;
        if (operation < 2 || oracle.empty()) {
            auto index = rng() % (oracle.size() + 1);
            int value = int(rng() % 10000);
            custom.Insert(index ? custom.begin() + index : custom.begin(), value);
            oracle.insert(oracle.begin() + index, value);
        } else if (operation == 2) {
            auto index = rng() % oracle.size();
            custom.Erase(custom.begin() + index);
            oracle.erase(oracle.begin() + index);
        } else if (operation == 3) {
            auto n = rng() % 50;
            custom.Resize(n);
            oracle.resize(n);
        } else {
            auto n = rng() % 100;
            custom.Reserve(n);
            oracle.reserve(n);
        }
        assert(custom.Size() == oracle.size());
        for (std::size_t j = 0; j < oracle.size(); ++j)
            assert(custom[j] == oracle[j]);
    }
    Vector<std::string> alias;
    alias.PushBack("retained");
    alias.PushBack(alias[0]);
    assert(alias[0] == "retained" && alias[1] == "retained");
    Vector<std::unique_ptr<int>> movable;
    movable.EmplaceBack(std::make_unique<int>(3));
    movable.Emplace(movable.begin(), std::make_unique<int>(4));
    assert(*movable[0] == 4 && *movable[1] == 3);
    for (int i = 0; i < 10000; ++i) {
        Nucleotide n{"ATGC"[rng() % 4],  rng() % 3300000001ULL, int(rng() % 46 + 1),
                     int(rng() % 25001), bool(rng() % 2),       char(rng() % 256)};
        auto d = Decompress(Compress(n));
        assert(
            std::tie(n.Symbol, n.Position, n.ChromosomeNum, n.GeneNum, n.IsMarked, n.ServiceInfo) ==
            std::tie(d.Symbol, d.Position, d.ChromosomeNum, d.GeneNum, d.IsMarked, d.ServiceInfo));
    }
    std::map<int, std::vector<std::string>> source{{2, {"abc", std::string("a\0b", 3)}}, {-1, {}}},
        target;
    std::stringstream buffer;
    Serialize(source, buffer);
    Deserialize(buffer, target);
    assert(source == target);
    std::stringstream short_input(buffer.str().substr(0, buffer.str().size() - 1));
    Throws([&] { Deserialize(short_input, target); });
    assert(source == target);
}
void PrintersAndPhonebook() {
    std::ostringstream formatted;
    formatted << std::hex << std::showbase << std::showpos << std::setfill('x') << std::setw(5);
    PrintJsonArray(formatted).Number(255).Number(-3).Boolean(true).String("ok").EndArray();
    assert(formatted.str() == "[255,-3,true,\"ok\"]");
    std::ostringstream out;
    PrintJsonObject(out)
        .Key("a")
        .BeginArray()
        .Number(-3)
        .Boolean(true)
        .BeginObject()
        .Key("x\n")
        .String("\"\\\t")
        .EndObject()
        .Null()
        .EndArray()
        .EndObject();
    auto parsed = json::Parse(out.str());
    assert(parsed.At("a").AsArray()[2].At("x\n").AsString() == "\"\\\t");
    std::ostringstream automatic;
    PrintJsonArray(automatic).BeginObject().Key("unfinished");
    assert(automatic.str() == "[{\"unfinished\":null}]");
    PhoneBook book({{"B", {}, {}},
                    {"Anna", Date{2000, 2, 29}, {"+1", ""}},
                    {"An", {}, {}},
                    {"Anna", {}, {}},
                    {"", {}, {}},
                    {"Anne", {}, {}}});
    assert(book.FindByNamePrefix("An").size() == 4 && book.FindByNamePrefix("Anna").size() == 2);
    assert(book.FindByNamePrefix("").size() == 6 && book.FindByNamePrefix("Z").size() == 0);
    std::stringstream wire;
    book.SaveTo(wire);
    auto loaded = DeserializePhoneBook(wire);
    assert(loaded.FindByNamePrefix("Anna").size() == 2);
    auto first = loaded.FindByNamePrefix("Anna").begin();
    assert(first->birthday->day == 29 && first->phones[1].empty());
    std::stringstream truncated(std::string("\x0a\x08xx", 4));
    Throws([&] { DeserializePhoneBook(truncated); });
    const auto company = MergeCompanies(
        {{1, json::Parse(R"({"names":[{"value":"old"}],"urls":["a"]})")},
         {2, json::Parse(R"({"names":[{"value":"new"}],"phones":["7"]})")},
         {3,
          json::Parse(
              R"({"names":[{"value":"new"},{"value":"alias"}],"address":{"formatted":"here"}})")}},
        {{1, 1}, {2, 3}, {3, 3}});
    assert(company.At("names").AsArray().size() == 2 &&
           company.At("urls").AsArray()[0].AsString() == "a");
    assert(company.At("phones").AsArray()[0].AsString() == "7" &&
           company.At("address").At("formatted").AsString() == "here");
}
void Routing() {
    std::mt19937 rng(91);
    constexpr double infinity = 1e90;
    // Independent dense stop-to-stop edge model, followed by Floyd-Warshall.
    // Production code instead uses onboard nodes and Dijkstra.
    for (int trial = 0; trial < 400; ++trial) {
        museum::brown::Transport catalog;
        int n = 2 + rng() % 7;
        std::vector<std::vector<int>> roads(n, std::vector<int>(n));
        for (int i = 0; i < n; ++i) {
            museum::brown::Stop stop{std::to_string(i), double(i), double(i), {}};
            for (int j = 0; j < n; ++j)
                stop.road_distances[std::to_string(j)] = roads[i][j] = i == j ? 0 : rng() % 10000;
            catalog.AddStop(std::move(stop));
        }
        const double wait = rng() % 6, velocity = 10 + rng() % 80;
        std::vector<std::vector<double>> distance(n, std::vector<double>(n, infinity));
        for (int i = 0; i < n; ++i)
            distance[i][i] = 0;
        const auto connect = [&](const std::vector<std::string> &route) {
            for (std::size_t i = 0; i < route.size(); ++i) {
                double time = wait;
                for (std::size_t j = i + 1; j < route.size(); ++j) {
                    time += roads[std::stoi(route[j - 1])][std::stoi(route[j])] * 0.06 / velocity;
                    auto &value = distance[std::stoi(route[i])][std::stoi(route[j])];
                    value = std::min(value, time);
                }
            }
        };
        for (int b = 0, count = rng() % 6; b < count; ++b) {
            museum::brown::Bus bus{std::to_string(b), {}, bool(rng() % 2)};
            for (int k = 0, length = 1 + rng() % 7; k < length; ++k)
                bus.stops.push_back(std::to_string(rng() % n));
            connect(bus.stops);
            if (!bus.is_roundtrip) {
                auto reverse = bus.stops;
                std::reverse(reverse.begin(), reverse.end());
                connect(reverse);
            }
            catalog.AddBus(std::move(bus));
        }
        for (int k = 0; k < n; ++k)
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    distance[i][j] = std::min(distance[i][j], distance[i][k] + distance[k][j]);
        Router router(catalog, wait, velocity);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                auto route = router.Route(std::to_string(i), std::to_string(j));
                if (distance[i][j] >= infinity)
                    assert(route.Contains("error_message"));
                else {
                    assert(std::abs(route.At("total_time").AsNumber() - distance[i][j]) < 1e-8);
                    double sum = 0;
                    bool riding = false;
                    for (const auto &item : route.At("items").AsArray()) {
                        sum += item.At("time").AsNumber();
                        if (item.At("type").AsString() == "Wait") {
                            assert(!riding);
                            riding = true;
                        } else {
                            assert(riding && item.At("span_count").AsNumber() >= 1);
                            riding = false;
                        }
                    }
                    assert(!riding && std::abs(sum - distance[i][j]) < 1e-8);
                }
            }
        assert(router.Route("missing", "0").Contains("error_message"));
    }
}
ss::Position P(const std::string &s) { return ss::Position::FromString(s); }
double Number(ss::Sheet &s, const std::string &p) { return std::get<double>(s.GetValue(P(p))); }
void Spreadsheet() {
    assert(P("XFD16384").ToString() == "XFD16384");
    for (const std::string s : {"", "a1", "A0", "A01", "XFE1", "A16385", "A1junk", "AAAAAAAA1"})
        assert(!P(s).IsValid());
    for (int i = 0; i < 16384; ++i) {
        ss::Position p{i, 16383 - i};
        assert(P(p.ToString()) == p);
    }
    ss::Sheet sheet;
    sheet.SetCell(P("A1"), "42");
    sheet.SetCell(P("B1"), "43");
    sheet.SetCell(P("A2"), "=A1");
    sheet.SetCell(P("B2"), "= A2 + B1");
    assert(Number(sheet, "B2") == 85);
    sheet.InsertCols(1);
    assert(sheet.GetCell(P("C2"))->GetText() == "=A2+C1" && Number(sheet, "C2") == 85);
    sheet.DeleteCols(0);
    assert(sheet.GetCell(P("B2"))->GetText() == "=#REF!+B1");
    assert(std::get<ss::FormulaError>(sheet.GetValue(P("B2"))) == ss::FormulaError::Ref);
    sheet.SetCell(P("A1"), "=2*-(-3+1)+8/2");
    assert(Number(sheet, "A1") == 8);
    sheet.SetCell(P("A2"), "=A1*2");
    sheet.SetCell(P("A3"), "=A2+A1+A2");
    assert(Number(sheet, "A3") == 40);
    Throws([&] { sheet.SetCell(P("A1"), "=A3"); });
    assert(Number(sheet, "A1") == 8);
    sheet.SetCell(P("A1"), "=5");
    assert(Number(sheet, "A3") == 25);
    sheet.ClearCell(P("A1"));
    assert(Number(sheet, "A3") == 0);
    for (const auto &bad : {"=1+", "=()", "=1 2", "=(1", "=1)", "=A", "=1**2", "=1.2.3"})
        Throws([&] { sheet.SetCell(P("A1"), bad); });
    sheet.SetCell(P("A1"), "'5");
    assert(Number(sheet, "A3") == 25);
    sheet.SetCell(P("A1"), "hello");
    assert(std::get<ss::FormulaError>(sheet.GetValue(P("A3"))) == ss::FormulaError::Value);
    sheet.SetCell(P("A1"), "=1/0");
    assert(std::get<ss::FormulaError>(sheet.GetValue(P("A3"))) == ss::FormulaError::Div0);
    sheet.SetCell(P("A1"), "=XFE1");
    assert(std::get<ss::FormulaError>(sheet.GetValue(P("A1"))) == ss::FormulaError::Ref);
    sheet.SetCell(P("XFD16384"), "edge");
    Throws([&] { sheet.InsertRows(0); });
    assert(sheet.GetCell(P("XFD16384"))->GetText() == "edge");
    sheet.ClearCell(P("XFD16384"));
    sheet.InsertRows(0, 2);
    sheet.DeleteRows(0, 2);
    assert(sheet.GetCell(P("A2"))->GetText() == "=A1*2");
    // Long chains exercise nonrecursive evaluation and cache invalidation.
    ss::Sheet chain;
    for (int i = 0; i < 6000; ++i)
        chain.SetCell({i, 0}, i == 5999 ? "=1" : "=A" + std::to_string(i + 2) + "+1");
    assert(Number(chain, "A1") == 6000);
    chain.SetCell({5999, 0}, "=2");
    assert(Number(chain, "A1") == 6001);
    // Independent arithmetic oracle over acyclic dependency graphs.
    std::mt19937 rng(181);
    ss::Sheet random;
    std::vector<double> expected;
    for (int i = 0; i < 1500; ++i) {
        double value;
        std::string expression;
        if (i < 5 || rng() % 3 == 0) {
            value = rng() % 100;
            expression = "=" + std::to_string(int(value));
        } else {
            auto a = rng() % i, b = rng() % i;
            value = expected[a] / 2 + expected[b] / 3;
            expression = "=A" + std::to_string(a + 1) + "/2+A" + std::to_string(b + 1) + "/3";
        }
        expected.push_back(value);
        random.SetCell({i, 0}, expression);
        assert(std::abs(std::get<double>(random.GetValue({i, 0})) - value) < 1e-8);
    }
}
// Random rewrites, clears and structural edits after values were cached: every
// value must equal that of a sheet rebuilt from the resulting texts.
void SpreadsheetEdits() {
    std::mt19937 rng(4242);
    const auto position = [&](int rows, int cols) {
        return ss::Position{int(rng() % rows), int(rng() % cols)};
    };
    for (int trial = 0; trial < 60; ++trial) {
        ss::Sheet sheet;
        for (int step = 0; step < 80; ++step) {
            const auto target = position(6, 4);
            try {
                switch (rng() % 6) {
                case 0:
                case 1:
                case 2: {
                    std::string text = "=";
                    for (int term = 0, terms = 1 + rng() % 3; term < terms; ++term) {
                        if (term)
                            text += "+-*/"[rng() % 4];
                        text += rng() % 3 ? position(6, 4).ToString() : std::to_string(rng() % 5);
                    }
                    sheet.SetCell(target, rng() % 4 ? text : std::to_string(rng() % 20));
                    break;
                }
                case 3:
                    sheet.ClearCell(target);
                    break;
                case 4:
                    rng() % 2 ? sheet.InsertRows(rng() % 6) : sheet.InsertCols(rng() % 4);
                    break;
                default:
                    rng() % 2 ? sheet.DeleteRows(rng() % 6) : sheet.DeleteCols(rng() % 4);
                }
            } catch (const ss::CircularDependencyException &) {
            }
            for (int read = 0; read < 4; ++read)
                (void)sheet.GetValue(position(9, 7));
            const auto size = sheet.GetPrintableSize();
            ss::Sheet fresh;
            for (int row = 0; row < size.rows; ++row)
                for (int col = 0; col < size.cols; ++col)
                    if (const auto *cell = sheet.GetCell({row, col}))
                        fresh.SetCell({row, col}, cell->GetText());
            for (int row = 0; row < size.rows; ++row)
                for (int col = 0; col < size.cols; ++col)
                    assert(sheet.GetValue({row, col}) == fresh.GetValue({row, col}));
        }
    }
}
int main() {
    Foundations();
    PrintersAndPhonebook();
    Routing();
    Spreadsheet();
    SpreadsheetEdits();
    std::cout
        << "Black: lifetimes/exceptions, 15000 vector operations, 10000 packed records, "
           "400 routing oracles, 16384 cell indices, 6000-cell chains, 1500 formula oracles, "
           "4800 random sheet edits PASS\n";
}
