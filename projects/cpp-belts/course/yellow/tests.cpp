#include "budget.hpp"
#include "buses/bus_manager.h"
#include "buses/query.h"
#include "contract_tests.hpp"
#include "events/condition_parser.h"
#include "events/database.h"
#include "exercises.hpp"
#include "oop.hpp"
#include "phone_number.h"
#include "rectangle.h"
#include "sum_reverse_sort.h"
#include <climits>
#include <random>
#include <sstream>
#include <type_traits>
#define CHECK(...)                                                                                 \
    do {                                                                                           \
        if (!(__VA_ARGS__))                                                                        \
            throw std::runtime_error("check failed at line " + std::to_string(__LINE__) +          \
                                     ": " #__VA_ARGS__);                                           \
    } while (false)
template <class E, class F> void Throws(F f) {
    bool seen = false;
    try {
        f();
    } catch (const E &) {
        seen = true;
    }
    CHECK(seen);
}
struct Capture {
    std::ostringstream out;
    std::streambuf *old = std::cout.rdbuf(out.rdbuf());
    ~Capture() { std::cout.rdbuf(old); }
};
namespace yellow {
std::vector<std::string> sent;
void SendSms(const std::string &number, const std::string &message) {
    sent.push_back("sms:" + number + ":" + message);
}
void SendEmail(const std::string &address, const std::string &message) {
    sent.push_back("email:" + address + ":" + message);
}
} // namespace yellow
using namespace yellow;
void TestMatrix() {
    Matrix a(2, 3);
    a.At(1, 2) = 9;
    CHECK(a.GetNumRows() == 2 && a.GetNumColumns() == 3);
    CHECK((a + a).At(1, 2) == 18);
    const Matrix &c = a;
    CHECK(c.At(1, 2) == 9);
    Throws<std::out_of_range>([] { Matrix m(-1, 2); });
    Throws<std::out_of_range>([&] { a.At(2, 0); });
    Throws<std::out_of_range>([&] { c.At(0, -1); });
    Throws<std::invalid_argument>([&] {
        auto m = a + Matrix(3, 2);
        (void)m;
    });
    CHECK(Matrix(0, 7) == Matrix(2, 0));
    CHECK(Matrix(0, 7) + Matrix(2, 0) == Matrix{});
    a.Reset(1, 2);
    CHECK(a.At(0, 0) == 0);
    std::stringstream stream("2 2 1 2 3 4");
    Matrix m;
    stream >> m;
    std::ostringstream out;
    out << m;
    CHECK(out.str() == "2 2\n1 2\n3 4");
    Matrix big(1, 1);
    big.At(0, 0) = INT_MAX;
    Throws<std::overflow_error>([&] {
        auto x = big + big;
        (void)x;
    });
}
void TestTemperature() {
    std::vector<int> v(1000000, 100000000);
    v[0] = -100000000;
    auto indices = white::AboveAverage(v);
    CHECK(indices.size() == 999999 && indices.front() == 1);
    CHECK(white::AboveAverage({-8, -6, -4, -2}) == std::vector<std::size_t>({2, 3}));
}
void TestRegions() {
    Region a{"a", "b", {{Lang::DE, "c"}}, 100}, b = a;
    b.population = 101;
    CHECK(FindMaxRepetitionCount({a, b, a}) == 2);
    CHECK(FindMaxRepetitionCount({}) == 0);
    for (int field = 0; field < 4; ++field) {
        b = a;
        if (field == 0)
            b.std_name = "z";
        if (field == 1)
            b.parent_std_name = "z";
        if (field == 2)
            b.names[Lang::FR] = "z";
        if (field == 3)
            b.population++;
        CHECK(FindMaxRepetitionCount({a, b}) == 1);
    }
}
void TestTasks() {
    TeamTasks tasks;
    std::vector<int> statuses;
    std::mt19937 rng(51);
    for (int step = 0; step < 500; ++step) {
        if (statuses.empty() || rng() % 3 == 0) {
            tasks.AddNewTask("person");
            statuses.push_back(0);
        } else {
            int limit = static_cast<int>(rng() % 20) + 1;
            std::sort(statuses.begin(), statuses.end());
            TasksInfo changed, untouched;
            int moved = 0;
            for (int &status : statuses) {
                if (status == 3)
                    continue;
                if (moved < limit) {
                    ++status;
                    ++changed[static_cast<TaskStatus>(status)];
                    ++moved;
                } else
                    ++untouched[static_cast<TaskStatus>(status)];
            }
            auto [actual_changed, actual_untouched] = tasks.PerformPersonTasks("person", limit);
            CHECK(changed == actual_changed && untouched == actual_untouched);
        }
        TasksInfo expected;
        for (int s : statuses)
            ++expected[static_cast<TaskStatus>(s)];
        CHECK(expected == tasks.GetPersonTasksInfo("person"));
    }
    CHECK(tasks.PerformPersonTasks("absent", 10) == std::make_tuple(TasksInfo{}, TasksInfo{}));
}
void TestSquare() {
    CHECK(Sqr(std::vector<int>{-3, 0, 4}) == std::vector<int>({9, 0, 16}));
    std::vector<std::map<int, std::pair<int, int>>> nested = {{{7, {-2, 3}}}};
    auto result = Sqr(nested);
    CHECK(result[0].at(7) == std::make_pair(4, 9));
    CHECK(nested[0].at(7).first == -2);
}
void TestReference() {
    std::map<int, std::string> m{{1, "x"}};
    GetRefStrict(m, 1) = "y";
    CHECK(m.at(1) == "y");
    Throws<std::runtime_error>([&] { GetRefStrict(m, 2); });
    CHECK(m.size() == 1);
}
void TestRootContract() {
    contracts::RootCounter(GetDistinctRealRootCount);
    Throws<std::runtime_error>(
        [] { contracts::RootCounter([](double, double, double) { return 2; }); });
}
void TestPersonContract() {
    contracts::Person<Person>();
    struct Broken {
        void ChangeFirstName(int, const std::string &) {}
        void ChangeLastName(int, const std::string &) {}
        std::string GetFullName(int) { return "Incognito"; }
    };
    Throws<std::runtime_error>([] { contracts::Person<Broken>(); });
}
void TestRationalContract() {
    contracts::Rational<Rational>();
    struct Broken {
        int n = 0, d = 1;
        Broken() = default;
        Broken(int a, int b) : n(a), d(b) {}
        int Numerator() const { return n; }
        int Denominator() const { return d; }
    };
    Throws<std::runtime_error>([] { contracts::Rational<Broken>(); });
}
void TestPalindromeContract() {
    contracts::Palindrome(IsPalindrom);
    Throws<std::runtime_error>([] {
        contracts::Palindrome([](const std::string &s) {
            std::string trimmed;
            for (char c : s)
                if (c != ' ')
                    trimmed += c;
            return white::IsPalindrom(trimmed);
        });
    });
    Throws<std::runtime_error>([] {
        contracts::Palindrome(
            [](const std::string &s) { return s.size() < 2 || s.front() == s.back(); });
    });
}
void TestSeparateFunctions() {
    CHECK(Sum(-7, 9) == 2);
    CHECK(Reverse("abcd") == "dcba");
    std::vector<int> v{2, 1, 2};
    Sort(v);
    CHECK(v == std::vector<int>({1, 2, 2}));
}
void TestPhone() {
    PhoneNumber p("+1-22-local-part");
    CHECK(p.GetCountryCode() == "1" && p.GetCityCode() == "22" &&
          p.GetLocalNumber() == "local-part");
    CHECK(p.GetInternationalNumber() == "+1-22-local-part");
    for (const auto &s : {"", "1-2-3", "+-2-3", "+1--3", "+1-2-", "+12"})
        Throws<std::invalid_argument>([&] { PhoneNumber bad(s); });
}
void TestRectangle() {
    const Rectangle r(3, 5);
    CHECK(r.GetArea() == 15 && r.GetPerimeter() == 16 && r.GetWidth() == 3 && r.GetHeight() == 5);
}
void TestBusInterfaces() {
    buses::Query q;
    std::istringstream in("NEW_BUS B 2 x y ALL_BUSES");
    in >> q;
    CHECK(q.type == buses::QueryType::NewBus && q.stops.size() == 2);
    buses::BusManager b;
    b.AddBus(q.bus, q.stops);
    in >> q;
    CHECK(q.type == buses::QueryType::AllBuses && q.stops.empty());
    b.AddBus("A", {"y"});
    std::ostringstream out;
    out << b.GetBusesForStop("y") << '\n' << b.GetStopsForBus("B") << '\n' << b.GetAllBuses();
    CHECK(out.str() == "B A\nStop x: no interchange\nStop y: A\nBus A: y\nBus B: x y");
}
void TestVectorPart() {
    Capture c;
    PrintVectorPart({6, 1, 8, -5, 9});
    CHECK(c.out.str() == "8 1 6");
    c.out.str("");
    PrintVectorPart({-1, 2});
    CHECK(c.out.str().empty());
    PrintVectorPart({});
    CHECK(c.out.str().empty());
    PrintVectorPart({1, 2});
    CHECK(c.out.str() == "2 1");
}
void TestGreater() {
    CHECK(FindGreaterElements(std::set<int>{1, 3, 5}, 3) == std::vector<int>({5}));
    CHECK(FindGreaterElements(std::set<int>{}, 1).empty());
    CHECK(FindGreaterElements(std::set<std::string>{"a", "b"}, std::string("z")).empty());
}
void TestWords() {
    CHECK(SplitIntoWords("One Two Three") == std::vector<std::string>({"One", "Two", "Three"}));
    CHECK(SplitIntoWords("One") == std::vector<std::string>({"One"}));
}
void TestDuplicates() {
    std::vector<int> v{2, 3, 2, 1, 3};
    RemoveDuplicates(v);
    CHECK(v == std::vector<int>({1, 2, 3}));
    std::vector<std::string> s{"x", "x"};
    RemoveDuplicates(s);
    CHECK(s.size() == 1);
    std::vector<int> empty;
    RemoveDuplicates(empty);
}
void TestMergeSort() {
    std::mt19937 rng(761);
    for (int n = 0; n < 300; ++n) {
        std::vector<int> v(n);
        for (auto &x : v)
            x = static_cast<int>(rng() % 37) - 18;
        auto expected = v;
        std::stable_sort(expected.begin(), expected.end());
        auto binary = v, thirds = v;
        MergeSort(binary.begin(), binary.end());
        ternary::MergeSort(thirds.begin(), thirds.end());
        CHECK(binary == expected);
        CHECK(thirds == expected);
    }
}
void TestDemographics() {
    using namespace demographics;
    Capture c;
    PrintStats({{20, Gender::FEMALE, true},
                {40, Gender::FEMALE, false},
                {30, Gender::MALE, true},
                {50, Gender::MALE, false}});
    CHECK(c.out.str() ==
          "Median age = 40\nMedian age for females = 40\nMedian age for males = 50\nMedian age for "
          "employed females = 20\nMedian age for unemployed females = 40\nMedian age for employed "
          "males = 30\nMedian age for unemployed males = 50\n");
    c.out.str("");
    PrintStats({});
    CHECK(c.out.str().find("Median age = 0\n") == 0);
}
void TestBudget() {
    CHECK(DayIndex({1700, 1, 1}) == 0);
    CHECK(DayIndex({2000, 3, 1}) - DayIndex({2000, 2, 28}) == 2);
    CHECK(DayIndex({1900, 3, 1}) - DayIndex({1900, 2, 28}) == 1);
    CHECK(DayIndex({2099, 12, 31}) == 146096);
    Budget b;
    b.Earn({2000, 2, 28}, {2000, 3, 1}, 30);
    CHECK(b.ComputeIncome({2000, 2, 29}, {2000, 2, 29}) == 10);
    b.Earn({2000, 2, 29}, {2000, 2, 29}, 7);
    CHECK(b.ComputeIncome({2000, 1, 1}, {2001, 1, 1}) == 37);
}
void TestPrefixBudget() {
    PrefixBudget b;
    b.Earn({1700, 1, 1}, 5);
    b.Earn({1700, 1, 1}, 9);
    b.Earn({2099, 12, 31}, 11);
    b.Seal();
    b.Seal();
    CHECK(b.ComputeIncome({1700, 1, 1}, {2099, 12, 31}) == 25);
    CHECK(b.ComputeIncome({1700, 1, 2}, {2099, 12, 30}) == 0);
    Throws<std::logic_error>([&] { b.Earn({2000, 1, 1}, 1); });
}
void TestNearest() {
    const std::set<int> s{INT_MIN, -4, 4, INT_MAX};
    CHECK(*FindNearestElement(s, 0) == -4);
    CHECK(*FindNearestElement(s, INT_MIN) == INT_MIN);
    CHECK(*FindNearestElement(s, INT_MAX) == INT_MAX);
    const std::set<int> empty;
    CHECK(FindNearestElement(empty, 5) == empty.end());
}
void TestPrefix() {
    std::vector<std::string> words = {"", "a", "aa", "ab", "az", "b", "ba", "zz"};
    for (const auto &p :
         std::vector<std::string>{"", "a", "aa", "ac", "az", "b", "y", "zz", "zzz"}) {
        auto [first, last] = FindStartsWith(words.begin(), words.end(), p);
        std::vector<std::string> expected;
        for (const auto &s : words)
            if (s.compare(0, p.size(), p) == 0)
                expected.push_back(s);
        CHECK(std::vector<std::string>(first, last) == expected);
        CHECK(first == std::lower_bound(words.begin(), words.end(), p));
    }
    CHECK(FindStartsWith(words.begin(), words.end(), 'a') ==
          FindStartsWith(words.begin(), words.end(), std::string("a")));
}
void TestAnimal() {
    static_assert(std::is_base_of_v<Animal, Dog>);
    Dog d("Spot");
    Capture c;
    d.Bark();
    CHECK(d.Name == "Spot" && c.out.str() == "Spot barks: woof!\n");
}
void TestNotifiers() {
    static_assert(std::is_abstract_v<INotifier>);
    sent.clear();
    SmsNotifier sms("000");
    EmailNotifier email("test.invalid");
    INotifier &a = sms;
    a.Notify("hi");
    INotifier &b = email;
    b.Notify("hello");
    CHECK(sent == std::vector<std::string>({"sms:000:hi", "email:test.invalid:hello"}));
}
void TestFigures() {
    Rect r(2, 3);
    Triangle t(3, 4, 5);
    Circle c(5);
    CHECK(r.Area() == 6 && r.Perimeter() == 10);
    CHECK(t.Area() == 6 && t.Perimeter() == 12);
    CHECK(std::abs(c.Area() - 78.5) < 1e-12);
    std::istringstream in("CIRCLE 5");
    auto f = CreateFigure(in);
    CHECK(f->Name() == "CIRCLE");
}
void TestRefactor() {
    using namespace refactoring;
    Teacher t("T", "math");
    Student s("S", "song");
    Policeman p("P");
    Capture c;
    VisitPlaces(t, {"X"});
    p.Check(s);
    VisitPlaces(s, {"Y"});
    t.Teach();
    s.Learn();
    CHECK(c.out.str() == "Teacher: T walks to: X\nPoliceman: P checks Student. Student's name is: "
                         "S\nStudent: S walks to: Y\nStudent: S sings a song: song\nTeacher: T "
                         "teaches: math\nStudent: S learns\n");
}
void TestEvents() {
    using namespace events;
    const Date early(2000, 1, 1), late(2001, 1, 1);
    Database db;
    db.Add(late, "z");
    db.Add(late, "a");
    db.Add(late, "z");
    db.Add(early, "old");
    CHECK(db.Last(late).second == "a");
    Throws<std::invalid_argument>([&] { db.Last(Date(1999, 1, 1)); });
    auto all = db.FindIf([](const Date &, const std::string &) { return true; });
    CHECK(all.size() == 3 && all[0].second == "old" && all[1].second == "z" &&
          all[2].second == "a");
    CHECK(db.RemoveIf([](const Date &, const std::string &e) { return e == "z"; }) == 1);
    db.Add(late, "z");
    CHECK(db.Last(late).second == "z");
    CHECK(db.RemoveIf([](const Date &, const std::string &) { return true; }) == 3);
    Throws<std::invalid_argument>([&] { db.Last(late); });
    for (const std::string op : {"<", "<=", ">", ">=", "==", "!="})
        for (int delta : {-1, 0, 1}) {
            std::istringstream input("date " + op + " 2000-01-01");
            auto node = ParseCondition(input);
            bool expected = op == "<"    ? delta < 0
                            : op == "<=" ? delta <= 0
                            : op == ">"  ? delta > 0
                            : op == ">=" ? delta >= 0
                            : op == "==" ? delta == 0
                                         : delta != 0;
            CHECK(node->Evaluate(Date(2000 + delta, 1, 1), "x") == expected);
            std::istringstream ein("event " + op + " \"b\"");
            auto enode = ParseCondition(ein);
            CHECK(enode->Evaluate(early, std::string(1, char('b' + delta))) == expected);
        }
    for (const auto &bad : {"date = 2000-01-01", "event == x", "(event == \"x\"",
                            "event == \"x\" garbage", "event == \"unterminated"})
        Throws<std::invalid_argument>([&] {
            std::istringstream s(bad);
            ParseCondition(s);
        });
    std::istringstream empty(" ");
    CHECK(ParseCondition(empty)->Evaluate(early, ""));
    std::istringstream expr("event == \"x\" OR event == \"y\" AND date > 2000-01-01");
    auto node = ParseCondition(expr);
    CHECK(node->Evaluate(early, "x"));
    CHECK(!node->Evaluate(early, "y"));
    CHECK(node->Evaluate(late, "y"));
    std::istringstream parens("(event == \"x\" OR event == \"y\") AND date > 2000-01-01");
    CHECK(!ParseCondition(parens)->Evaluate(early, "x"));
}
int main() {
    try {
        TestMatrix();
        TestTemperature();
        TestRegions();
        TestTasks();
        TestSquare();
        TestReference();
        TestRootContract();
        TestPersonContract();
        TestRationalContract();
        TestPalindromeContract();
        TestSeparateFunctions();
        TestPhone();
        TestRectangle();
        TestBusInterfaces();
        TestVectorPart();
        TestGreater();
        TestWords();
        TestDuplicates();
        TestMergeSort();
        TestDemographics();
        TestBudget();
        TestPrefixBudget();
        TestNearest();
        TestPrefix();
        TestAnimal();
        TestNotifiers();
        TestFigures();
        TestRefactor();
        TestEvents();
        std::cout << "PASS: 29 yellow API groups, random task/sort oracles, mutation checks and "
                     "event AST tests\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
