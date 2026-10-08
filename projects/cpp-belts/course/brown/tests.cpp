#include "budget.hpp"
#include "data.hpp"
#include "formats.hpp"
#include "graphics.hpp"
#include "ownership.hpp"
#include "services.hpp"
#include "transport.hpp"
#include <iostream>
#include <random>
namespace {
using namespace museum::brown;
void Check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void Throws(F f) {
    bool thrown = false;
    try {
        f();
    } catch (const std::exception &) {
        thrown = true;
    }
    Check(thrown, "expected exception");
}
void Data() {
    struct ConstantHash {
        std::size_t operator()(int) const { return 0; }
    };
    HashSet<int, ConstantHash> set(1);
    set.Add(1);
    set.Add(2);
    set.Add(1);
    set.Erase(2);
    Check(set.Has(1) && !set.Has(2) &&
              std::distance(set.GetBucket(1).begin(), set.GetBucket(1).end()) == 1,
          "hash collision and duplicate handling");
    Throws([] { HashSet<int> invalid(0); });
    Node a{1}, b{2}, c{3};
    b.left = &a;
    b.right = &c;
    a.parent = c.parent = &b;
    Check(Next(&a) == &b && Next(&b) == &c && !Next(&c) && !Next(nullptr),
          "BST successor both branches");
    std::unordered_set<Point3D, Hasher> points;
    points.insert({1, 2, 3});
    points.insert({1, 2, 3});
    points.insert({3, 2, 1});
    Check(points.size() == 2, "point equality and hash");
    std::set<std::size_t> hashes;
    for (int i = 0; i < 50; ++i)
        for (int j = 0; j < 50; ++j)
            hashes.insert(Hasher{}({i, j, 7}));
    Check(hashes.size() == 2500, "point grid dispersion");
    Person person{"Ann", 170, 65.5, {"city", "street", 2}};
    std::unordered_set<Person, PersonHasher> people{person};
    person.address.building = 3;
    people.insert(person);
    Check(people.size() == 2, "person address participates in hash and equality");
    Database db;
    Check(db.Put({"1", "one", "a", 10, 20}) && !db.Put({"1", "replacement", "b", 30, 40}),
          "database duplicate rejection");
    db.Put({"2", "two", "a", 10, 21});
    db.Put({"3", "three", "b", 12, 19});
    const auto *stable = db.GetById("1");
    int n = 0;
    db.RangeByTimestamp(10, 10, [&](const Record &) {
        ++n;
        return true;
    });
    Check(n == 2, "inclusive database range");
    n = 0;
    db.AllByUser("a", [&](const Record &) {
        ++n;
        return false;
    });
    Check(n == 1, "callback early stop");
    Check(db.Erase("2") && !db.Erase("2") && stable == db.GetById("1"),
          "index erase and stable pointers");
    n = 0;
    db.RangeByKarma(20, 21, [&](const Record &record) {
        Check(record.id == "1", "stale secondary index");
        ++n;
        return true;
    });
    Check(n == 1, "secondary index removal");
    int initialized = 0;
    LazyValue<std::unique_ptr<int>> lazy([&] {
        ++initialized;
        return std::make_unique<int>(42);
    });
    Check(!lazy.HasValue() && initialized == 0, "lazy initial state");
    std::vector<std::future<void>> jobs;
    for (int i = 0; i < 8; ++i)
        jobs.push_back(
            std::async(std::launch::async, [&] { Check(*lazy.Get() == 42, "lazy value"); }));
    for (auto &j : jobs)
        j.get();
    Check(initialized == 1 && lazy.HasValue(), "lazy one initialization");
    int calls = 0;
    LazyValue<int> retry([&] {
        if (++calls == 1)
            throw std::runtime_error("retry");
        return 7;
    });
    Throws([&] { retry.Get(); });
    Check(!retry.HasValue() && retry.Get() == 7, "lazy retry after exception");
    const Synchronized<int> constant(9);
    Check(constant.GetAccess().ref_to_value == 9, "const synchronized access");
    ConcurrentMap<std::string, int> map(5);
    jobs.clear();
    for (int i = 0; i < 4; ++i)
        jobs.push_back(std::async(std::launch::async, [&] {
            for (int j = 0; j < 10000; ++j)
                ++map[std::to_string(j % 13)].ref_to_value;
        }));
    for (auto &j : jobs)
        j.get();
    const auto &cm = map;
    Check(cm.Has("0") && !cm.Has("missing"), "const map membership");
    Throws([&] { cm.At("missing"); });
    int total = 0;
    for (auto [key, count] : cm.BuildOrdinaryMap())
        total += count;
    Check(total == 40000 && cm.At("0").ref_to_value == 3080, "const hash map snapshots");
    Polynomial<int> polynomial({1, 2, 0});
    Check(polynomial.Degree() == 1 && polynomial(3) == 7,
          "polynomial evaluation and trailing zeros");
    int absent = polynomial[99];
    Check(absent == 0 && polynomial.Degree() == 1, "coefficient read does not grow");
    polynomial[99] = 3;
    polynomial[5] = polynomial[99];
    polynomial[99] = 0;
    Check(polynomial.Degree() == 5 && static_cast<int>(polynomial[5]) == 3,
          "proxy assignment trims degree");
    polynomial -= polynomial;
    Check(polynomial.Degree() == -1, "self subtraction zero polynomial");
}
void Formats() {
    const std::string text = R"({"a":[true,false,null,-12.5e2],"s":"quote\"\n\uD83D\uDE00"})";
    auto node = json::Parse(text);
    auto parsed = json::Parse(json::Serialize(node));
    Check(parsed.At("a").AsArray()[3].AsNumber() == -1250 &&
              parsed.At("s").AsString() == "quote\"\n\xF0\x9F\x98\x80",
          "JSON escape and unicode roundtrip");
    for (const auto *bad :
         {"{\"a\":1,\"a\":2}", "[1,]", "01", "true x", "\"\\uD800\"", "1e999", "{\"a\" 1}"})
        Throws([&] { json::Parse(bad); });
    Throws([] { json::Node(1e40).AsInt(); });
    std::istringstream ini("[first]\na=b\n\n[first]\nc=d\n[other]\nx=y\n");
    const auto doc = Ini::Load(ini);
    Check(doc.SectionCount() == 2 && doc.GetSection("first").at("a") == "b" &&
              doc.GetSection("first").at("c") == "d",
          "INI merged sections");
    std::istringstream xml(
        "<july><spend category=\"food &amp; drink\" amount=\"2500\"></spend><spend amount=\"30\" "
        "category=\"bus\"></spend></july>");
    auto source = Xml::Load(xml);
    auto converted = XmlToJson(source);
    Check(converted.AsArray().size() == 2 && converted.AsArray()[0].At("amount").AsInt() == 2500 &&
              converted.AsArray()[0].At("category").AsString() == "food & drink",
          "XML spending conversion");
    std::ostringstream serialized;
    Xml::Print(JsonToXml(converted, "august"), serialized);
    std::istringstream restored(serialized.str());
    auto expenses = LoadFromXml(restored);
    Check(expenses.size() == 2 && expenses[1].amount == 30, "JSON XML roundtrip");
    std::istringstream json_input(json::Serialize(converted));
    Check(LoadFromJson(json_input)[0].amount == 2500, "JSON spending loader");
    StatsAggregators::Sum sum;
    StatsAggregators::Min min;
    StatsAggregators::Max max;
    StatsAggregators::Average average;
    StatsAggregators::Mode mode;
    Check(!min.Get() && !average.Get(), "empty aggregations");
    for (int value : {3, 1, 3, -1}) {
        sum.Process(value);
        min.Process(value);
        max.Process(value);
        average.Process(value);
        mode.Process(value);
    }
    Check(sum.Get() == 6 && min.Get() == -1 && max.Get() == 3 && average.Get() == 1.5 &&
              mode.Get() == 3,
          "all aggregations");
}
struct Provider {
    int completed = 0;
    void CancelOrComplete(const RAII::Booking<Provider> &) { ++completed; }
};
struct Book final : IBook {
    std::string name, content;
    Book(std::string n, std::string c) : name(std::move(n)), content(std::move(c)) {}
    const std::string &GetName() const override { return name; }
    const std::string &GetContent() const override { return content; }
};
struct Unpacker final : IBooksUnpacker {
    int calls = 0;
    BookPtr UnpackBook(const std::string &name) override {
        ++calls;
        return std::make_shared<Book>(name, std::string(name == "big" ? 100 : 3, 'x'));
    }
};
void Ownership() {
    UniquePtr<int> value(new int(3));
    auto other = std::move(value);
    Check(!value.Get() && *other == 3, "unique move");
    other.Reset(other.Get());
    Check(*other == 3, "unique self reset");
    int *raw = other.Release();
    Check(!other.Get(), "release ownership");
    delete raw;
    value.Reset(new int(7));
    other.Swap(value);
    Check(*other == 7, "unique swap");
    other = nullptr;
    Check(!other.Get(), "null assignment");
    Provider provider;
    {
        RAII::Booking<Provider> first(&provider, 1);
        auto second = std::move(first);
        RAII::Booking<Provider> third(&provider, 2);
        third = std::move(second);
        Check(provider.completed == 1 && third.GetId() == 1,
              "RAII move assignment releases old booking");
    }
    Check(provider.completed == 2, "exactly once RAII completion");
    std::istringstream animals("Tiger Fox Wolf");
    auto zoo = CreateZoo(animals);
    std::ostringstream voices;
    Process(zoo, voices);
    Check(voices.str() == "Rrrr\nTyaf\nWooo\n", "polymorphic zoo");
    Throws([] {
        std::istringstream bad("Dragon");
        CreateZoo(bad);
    });
    Throws([] { Product(Value(std::numeric_limits<int>::max()), Value(2))->Evaluate(); });
    auto expression = Product(Value(2), Sum(Value(3), Value(4)));
    Check(expression->Evaluate() == 14 && expression->ToString() == "(2)*((3)+(4))",
          "expression tree ownership and formatting");
    std::istringstream emails("a\nb\nhello\na\nc\nbye\nx\nb\nfiltered\n");
    std::ostringstream output;
    auto pipeline = PipelineBuilder(emails)
                        .FilterBy([](const Email &email) { return email.from == "a"; })
                        .CopyTo("c")
                        .Send(output)
                        .Build();
    pipeline->Run();
    Check(output.str() == "a\nb\nhello\na\nc\nhello\na\nc\nbye\n",
          "pipeline filter copy sender order");
    auto unpacker = std::make_shared<Unpacker>();
    auto cache = MakeCache(unpacker, {6});
    auto kept = cache->GetBook("a");
    cache->GetBook("b");
    cache->GetBook("a");
    cache->GetBook("c");
    Check(unpacker->calls == 3, "cache hit and recency");
    cache->GetBook("b");
    Check(unpacker->calls == 4 && kept->GetName() == "a",
          "LRU eviction preserves externally held book");
    cache->GetBook("big");
    cache->GetBook("c");
    Check(unpacker->calls == 6, "oversize clears cache");
    std::vector<std::future<BookPtr>> jobs;
    for (int i = 0; i < 8; ++i)
        jobs.push_back(std::async(std::launch::async, [&] { return cache->GetBook("same"); }));
    for (auto &j : jobs)
        Check(j.get()->GetName() == "same", "concurrent cache");
    Check(unpacker->calls == 7, "same-book concurrent unpack once");
}
struct Texture final : graphics::ITexture {
    graphics::Image image{"AB", "CD"};
    graphics::Size GetSize() const override { return {2, 2}; }
    const graphics::Image &GetImage() const override { return image; }
};
void Graphics() {
    using namespace geometry;
    Unit unit(Point{0, 0});
    Building building(Rectangle{{-1, -1}, {1, 1}});
    Tower tower(Circle{{3, 0}, 2});
    Fence fence(Segment{{0, 0}, {4, 0}});
    std::array<const GameObject *, 4> objects{&unit, &building, &tower, &fence};
    for (auto *a : objects)
        for (auto *b : objects)
            Check(Collide(*a, *b) == Collide(*b, *a), "double dispatch symmetry");
    Check(Collide(unit, building) && !Collide(unit, tower) && Collide(tower, fence),
          "geometry boundaries");
    Check(Collide(Segment{{0, 0}, {2, 2}}, Segment{{0, 2}, {2, 0}}) &&
              !Collide(Segment{{0, 0}, {1, 0}}, Segment{{2, 0}, {3, 0}}),
          "segment crossing and disjoint collinear");
    auto shape = graphics::MakeShape(graphics::ShapeType::Rectangle);
    shape->SetPosition({-1, 0});
    shape->SetSize({3, 3});
    auto texture = std::make_shared<Texture>();
    std::weak_ptr<Texture> observer = texture;
    shape->SetTexture(texture);
    auto clone = shape->Clone();
    texture.reset();
    shape.reset();
    Check(!observer.expired(), "clone shares texture lifetime");
    graphics::Image image(3, std::string(3, ' '));
    clone->Draw(image);
    Check(image == graphics::Image{"B. ", "D. ", ".. "}, "clipped texture and default fill");
    clone.reset();
    Check(observer.expired(), "last shape releases texture");
    auto ellipse = graphics::MakeShape(graphics::ShapeType::Ellipse);
    ellipse->SetSize({4, 4});
    graphics::Image pixels(4, std::string(4, ' '));
    ellipse->Draw(pixels);
    Check(pixels[0] == " .. " && pixels[1] == "....", "ellipse pixel centers");
}
void Services() {
    Demographics data({{"B", 20, 30, 'M'}, {"A", 10, 50, 'M'}, {"W", 40, 20, 'W'}});
    Check(data.Adults(20) == 2 && data.Wealthy(2) == 80 && data.Popular('M') == "A" &&
              !data.Popular('X'),
          "demographics independent indexes and lexical tie");
    using persons::Gender;
    std::vector<persons::Person> people{{31, Gender::MALE, false},   {40, Gender::FEMALE, true},
                                        {24, Gender::MALE, true},    {20, Gender::FEMALE, true},
                                        {80, Gender::FEMALE, false}, {78, Gender::MALE, false},
                                        {10, Gender::FEMALE, false}, {55, Gender::MALE, true}};
    auto stats = persons::ComputeStats(people);
    Check(stats.total == 40 && stats.females == 40 && stats.males == 55 &&
              stats.employed_females == 40 && stats.unemployed_females == 80 &&
              stats.employed_males == 55 && stats.unemployed_males == 78,
          "seven disjoint demographic groups");
    auto empty = persons::ComputeStats({});
    Check(empty.total == 0 && empty.unemployed_males == 0, "empty median");
    Check(persons::Median({1, 3}) == 3, "upper median for even group");
    std::ostringstream printed;
    persons::PrintStats(stats, printed);
    Check(printed.str().find("Median age for unemployed males = 78\n") != std::string::npos,
          "person stats output labels");
    TeamTasks tasks;
    for (int i = 0; i < 3; ++i)
        tasks.AddNewTask("u");
    auto [updated, untouched] = tasks.PerformPersonTasks("u", 2);
    Check(updated.at(TaskStatus::IN_PROGRESS) == 2 && untouched.at(TaskStatus::NEW) == 1,
          "task first advance");
    std::tie(updated, untouched) = tasks.PerformPersonTasks("u", 2);
    Check(updated.at(TaskStatus::IN_PROGRESS) == 1 && updated.at(TaskStatus::TESTING) == 1 &&
              untouched.at(TaskStatus::IN_PROGRESS) == 1,
          "one advance per task");
    tasks.PerformPersonTasks("u", 100);
    tasks.PerformPersonTasks("u", 100);
    std::tie(updated, untouched) = tasks.PerformPersonTasks("u", 100);
    Check(updated.empty() && untouched.empty() &&
              tasks.GetPersonTasksInfo("u").at(TaskStatus::DONE) == 3,
          "done excluded from untouched");
    std::vector<domains::Domain> roots;
    roots.emplace_back("ya.ru");
    roots.emplace_back("m.ya.ru");
    roots.emplace_back("com");
    domains::DomainChecker filter(roots.begin(), roots.end());
    Check(roots[0].GetReversedParts() == std::vector<std::string>{"ru", "ya"},
          "domain split has no spurious empty part and is reversed");
    Check(filter.RootCount() == 2 && filter.IsForbidden(domains::Domain("ya.ru")) &&
              filter.IsForbidden(domains::Domain("x.m.ya.ru")) &&
              !filter.IsForbidden(domains::Domain("ru")) &&
              !filter.IsForbidden(domains::Domain("notya.ru")),
          "domain self direction boundary and subsumption");
    HttpResponse response(HttpCode::Ok);
    response.AddHeader("X", "1")
        .AddHeader("X", "2")
        .AddHeader("Content-Length", "999")
        .SetContent("hello");
    std::ostringstream wire;
    wire << response;
    Check(wire.str() == "HTTP/1.1 200 OK\nX: 1\nX: 2\nContent-Length: 5\n\nhello",
          "HTTP header repetition and single correct content length");
    CommentServer server;
    Check(server.ServeRequest({"POST", "/add_user", "", {}}).Content() == "0", "create user");
    Check(server.ServeRequest({"POST", "/add_comment", "0 one", {}}).Code() == HttpCode::Ok,
          "first comment");
    server.ServeRequest({"POST", "/add_comment", "0 two", {}});
    Check(server.ServeRequest({"POST", "/add_comment", "0 three", {}}).Code() == HttpCode::Found,
          "third comment blocks");
    Check(server.ServeRequest({"POST", "/checkcaptcha", "0 42", {}}).Code() == HttpCode::Ok &&
              server.ServeRequest({"POST", "/add_comment", "0 four", {}}).Code() == HttpCode::Ok,
          "captcha unlocks");
    Check(server.ServeRequest({"DELETE", "/add_user", "", {}}).Code() == HttpCode::NotFound,
          "unknown HTTP request");
}
void BudgetAndTransport() {
    Check(DayIndex("2000-01-01") == 0 && DayIndex("2000-03-01") == 60 &&
              DayIndex("2001-01-01") == 366 && DayIndex("2099-12-31") == 36524,
          "calendar leap boundaries");
    Throws([] { DayIndex("2001-02-29"); });
    Budget budget;
    std::array<double, 28> earn{}, spend{};
    std::mt19937 random(48);
    auto date = [](int day) {
        return std::string("2000-01-") + (day < 9 ? "0" : "") + std::to_string(day + 1);
    };
    for (int i = 0; i < 5000; ++i) {
        int left = random() % 28, right = random() % 28;
        if (left > right)
            std::swap(left, right);
        double amount = random() % 1000;
        switch (i % 4) {
        case 0:
            budget.Earn(date(left), date(right), amount);
            for (int d = left; d <= right; ++d)
                earn[d] += amount / (right - left + 1);
            break;
        case 1:
            budget.Spend(date(left), date(right), amount);
            for (int d = left; d <= right; ++d)
                spend[d] += amount / (right - left + 1);
            break;
        case 2: {
            int tax = random() % 101;
            budget.PayTax(date(left), date(right), tax);
            for (int d = left; d <= right; ++d)
                earn[d] *= 1 - tax / 100.;
            break;
        }
        default: {
            double expected = 0;
            for (int d = left; d <= right; ++d)
                expected += earn[d] - spend[d];
            Check(std::abs(budget.ComputeIncome(date(left), date(right)) - expected) < 1e-7,
                  "affine lazy budget against day-by-day oracle");
        }
        }
    }
    Transport transport;
    transport.AddBus({"bus", {"a", "b"}, false});
    transport.AddStop({"a", 55, 37, {{"b", 100}}});
    transport.AddStop({"b", 55.001, 37, {{"a", 150}}});
    transport.AddStop({"empty", 0, 0, {}});
    auto stats = transport.BusStats("bus");
    Check(stats && stats->stop_count == 3 && stats->unique_stop_count == 2 &&
              stats->route_length == 250,
          "transport return leg and asymmetric roads");
    Check(transport.StopBuses("a")->count("bus") && transport.StopBuses("empty")->empty() &&
              !transport.StopBuses("missing"),
          "transport stop states");
    Check(Transport::GeographicDistance(transport.GetStop("a"), transport.GetStop("a")) == 0,
          "same-coordinate geographic zero");
    Throws([&] { transport.AddStop({"bad", 100, 0, {}}); });
    transport.AddStop({"self", 0, 0, {{"self", 50}}});
    Check(transport.RoadDistance("self", "self") == 50,
          "explicit self-road distance wins over zero fallback");
}
} // namespace
int main() {
    try {
        Data();
        Formats();
        Ownership();
        Graphics();
        Services();
        BudgetAndTransport();
        std::cout << "PASS brown: data, parser, concurrency, ownership, graphics, service "
                     "contracts; 5000 budget oracle operations\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL brown: " << e.what() << '\n';
        return 1;
    }
}
