#include "basics.hpp"
#include "database.hpp"
#include "person.hpp"
#include "rational.hpp"
#include <climits>
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
using namespace white;
namespace white {
int server_mode = 0;
std::string AskTimeServer() {
    if (server_mode == 1)
        throw std::system_error(std::error_code());
    if (server_mode == 2)
        throw std::logic_error("server bug");
    return "13:04:05";
}
} // namespace white
void TestFactorial() {
    CHECK(Factorial(-10) == 1);
    CHECK(Factorial(0) == 1);
    CHECK(Factorial(1) == 1);
    CHECK(Factorial(10) == 3628800);
}
void TestPalindrome() {
    for (int n = 0; n <= 10; ++n)
        for (int bits = 0; bits < (1 << n); ++bits) {
            std::string s;
            for (int i = 0; i < n; ++i)
                s += (bits >> i & 1) ? 'a' : 'b';
            std::string reverse(s.rbegin(), s.rend());
            CHECK(IsPalindrom(s) == (s == reverse));
        }
    CHECK(!IsPalindrom("a a "));
    CHECK(!IsPalindrom("Aa"));
    CHECK(IsPalindrom("a a"));
}
void TestFilter() {
    CHECK(PalindromFilter({"aba", "", "racecar", "aa", "abc"}, 3) ==
          std::vector<std::string>({"aba", "racecar"}));
    CHECK(PalindromFilter({"", "a"}, 0).size() == 2);
}
void TestMaximizer() {
    int n = 4;
    UpdateIfGreater(3, n);
    CHECK(n == 4);
    UpdateIfGreater(9, n);
    CHECK(n == 9);
    UpdateIfGreater(9, n);
    CHECK(n == 9);
}
void TestMoveStrings() {
    std::vector<std::string> a = {"a", "b"}, b = {"z"};
    MoveStrings(a, b);
    CHECK(a.empty());
    CHECK(b == std::vector<std::string>({"z", "a", "b"}));
    MoveStrings(a, b);
    CHECK(b.size() == 3);
}
void TestReverse() {
    std::vector<int> v = {1, 2, 3};
    Reverse(v);
    CHECK(v == std::vector<int>({3, 2, 1}));
    Reverse(v);
    CHECK(v == std::vector<int>({1, 2, 3}));
    v.clear();
    Reverse(v);
    CHECK(v.empty());
}
void TestReversed() {
    const std::vector<int> v = {1, 2, 3, 4};
    CHECK(Reversed(v) == std::vector<int>({4, 3, 2, 1}));
    CHECK(v.front() == 1);
    CHECK(Reversed({}).empty());
}
void TestMapValues() {
    CHECK(BuildMapValuesSet({{1, "x"}, {2, "y"}, {3, "x"}}) == std::set<std::string>({"x", "y"}));
    CHECK(BuildMapValuesSet({}).empty());
}
void TestSortedStrings() {
    SortedStrings s;
    s.AddString("z");
    s.AddString("a");
    s.AddString("a");
    CHECK(s.GetSortedStrings() == std::vector<std::string>({"a", "a", "z"}));
}
void TestPerson() {
    Person p;
    CHECK(p.GetFullName(20) == "Incognito");
    p.ChangeFirstName(10, "A");
    CHECK(p.GetFullName(10) == "A with unknown last name");
    p.ChangeLastName(8, "B");
    CHECK(p.GetFullName(8) == "B with unknown first name");
    CHECK(p.GetFullName(10) == "A B");
    p.ChangeFirstName(5, "C");
    CHECK(p.GetFullName(9) == "C B");
    CHECK(p.GetFullName(10) == "A B");
}
void TestPersonHistory() {
    Person p;
    p.ChangeFirstName(1, "A");
    p.ChangeFirstName(2, "A");
    p.ChangeFirstName(4, "B");
    p.ChangeFirstName(6, "A");
    p.ChangeLastName(1, "Z");
    CHECK(p.GetFullNameWithHistory(6) == "A (B, A) Z");
    p.ChangeFirstName(3, "C");
    CHECK(p.GetFullNameWithHistory(6) == "A (B, C, A) Z");
}
void TestPersonBirth() {
    static_assert(!std::is_default_constructible_v<birth::Person>);
    const birth::Person c("A", "B", 10);
    CHECK(c.GetFullName(9) == "No person");
    CHECK(c.GetFullName(10) == "A B");
    birth::Person p("A", "B", 10);
    p.ChangeFirstName(9, "C");
    CHECK(p.GetFullName(10) == "A B");
    p.ChangeLastName(10, "D");
    CHECK(p.GetFullNameWithHistory(10) == "A D");
}
void TestString() {
    ReversibleString s("abc");
    s.Reverse();
    const auto &c = s;
    CHECK(c.ToString() == "cba");
    s.Reverse();
    CHECK(s.ToString() == "abc");
    CHECK(ReversibleString{}.ToString().empty());
}
void TestInitialization() {
    Incognizable a, b = {}, c = {0}, d = {0, 1};
    CHECK(a.first == 0 && b.second == 0 && c.second == 0 && d.second == 1);
}
void TestLectureTitle() {
    static_assert(!std::is_constructible_v<LectureTitle, std::string, std::string, std::string>);
    static_assert(!std::is_constructible_v<LectureTitle, Course, Specialization, Week>);
    static_assert(!std::is_convertible_v<std::string, Specialization>);
    LectureTitle t(Specialization("cpp"), Course("white"), Week("four"));
    CHECK(t.specialization == "cpp" && t.course == "white" && t.week == "four");
}
void TestFunction() {
    Function f;
    f.AddPart('-', 10);
    f.AddPart('+', 36);
    const auto &c = f;
    CHECK(c.Apply(10) == 36);
    f.Invert();
    CHECK(f.Apply(46) == 20);
    f.Invert();
    CHECK(f.Apply(10) == 36);
}
void TestFunctionMultiply() {
    Function f;
    f.AddPart('*', 4);
    f.AddPart('-', 4);
    f.AddPart('+', 36);
    CHECK(f.Apply(10) == 72);
    f.Invert();
    CHECK(f.Apply(52) == 5);
    Function g;
    g.AddPart('/', -3);
    g.AddPart('+', 7);
    double y = g.Apply(24);
    g.Invert();
    CHECK(g.Apply(y) == 24);
}
void TestRational() {
    CHECK(Rational() == Rational(0, 1));
    CHECK(Rational(INT_MIN, 2).Numerator() == INT_MIN / 2);
    CHECK(Rational(INT_MIN, INT_MIN) == Rational(1, 1));
    for (int n = -8; n <= 8; ++n)
        for (int d = -8; d <= 8; ++d)
            if (d) {
                Rational a(n, d);
                CHECK(a.Denominator() > 0);
                CHECK(std::gcd(a.Numerator(), a.Denominator()) == 1);
                CHECK(std::int64_t(a.Numerator()) * d == std::int64_t(n) * a.Denominator());
                for (int k = -8; k <= 8; ++k)
                    for (int q = -8; q <= 8; ++q)
                        if (q) {
                            Rational b(k, q);
                            const auto equal_value = [](Rational r, std::int64_t p,
                                                        std::int64_t z) {
                                return std::int64_t(r.Numerator()) * z == p * r.Denominator();
                            };
                            CHECK(equal_value(a + b, std::int64_t(n) * q + std::int64_t(k) * d,
                                              std::int64_t(d) * q));
                            CHECK(equal_value(a - b, std::int64_t(n) * q - std::int64_t(k) * d,
                                              std::int64_t(d) * q));
                            CHECK(equal_value(a * b, n * k, d * q));
                            if (k)
                                CHECK(equal_value(a / b, n * q, d * k));
                            CHECK((a < b) == (static_cast<long double>(n) / d <
                                              static_cast<long double>(k) / q));
                        }
            }
    std::stringstream io("2/4 -3/-6");
    Rational a, b;
    io >> a >> b;
    CHECK(a == b);
    std::ostringstream out;
    out << a;
    CHECK(out.str() == "1/2");
    std::set<Rational> values{a, b, Rational(2, 3)};
    CHECK(values.size() == 2);
    std::stringstream bad("4|5");
    bad >> a;
    CHECK(bad.fail());
    CHECK(a == Rational(1, 2));
}
void TestRationalExceptions() {
    Throws<std::invalid_argument>([] { Rational x(1, 0); });
    Throws<std::domain_error>([] {
        auto x = Rational(1, 2) / Rational(0, 1);
        (void)x;
    });
}
void TestEnsureEqual() {
    EnsureEqual("a", "a");
    try {
        EnsureEqual("a", "b");
        CHECK(false);
    } catch (const std::runtime_error &e) {
        CHECK(std::string(e.what()) == "a != b");
    }
}
void TestTimeServer() {
    TimeServer server;
    server_mode = 1;
    CHECK(server.GetCurrentTime() == "00:00:00");
    server_mode = 0;
    CHECK(server.GetCurrentTime() == "13:04:05");
    server_mode = 1;
    CHECK(server.GetCurrentTime() == "13:04:05");
    server_mode = 2;
    Throws<std::logic_error>([&] { server.GetCurrentTime(); });
}
void TestDatabase() {
    const auto d = ParseDate("1-+1-+2");
    CHECK(d == Date({1, 1, 2}));
    CHECK(ParseDate("-1-1-1").year == -1);
    for (const auto &s : {"1---1-1", "1-1-1x", "1-1", "1--1-1", "1-1-0", "1-13-32"})
        Throws<std::invalid_argument>([&] { ParseDate(s); });
    Database db;
    db.Add(d, "z");
    db.Add(d, "a");
    db.Add(d, "a");
    std::ostringstream out;
    db.Print(out);
    CHECK(out.str() == "0001-01-02 a\n0001-01-02 z\n");
    CHECK(db.DeleteEvent(d, "z"));
    CHECK(!db.DeleteEvent(d, "z"));
    CHECK(db.DeleteDate(d) == 1);
    CHECK(db.DeleteDate(d) == 0);
}
int main() {
    try {
        TestFactorial();
        TestPalindrome();
        TestFilter();
        TestMaximizer();
        TestMoveStrings();
        TestReverse();
        TestReversed();
        TestMapValues();
        TestSortedStrings();
        TestPerson();
        TestPersonHistory();
        TestPersonBirth();
        TestString();
        TestInitialization();
        TestLectureTitle();
        TestFunction();
        TestFunctionMultiply();
        TestRational();
        TestRationalExceptions();
        TestEnsureEqual();
        TestTimeServer();
        TestDatabase();
        std::cout << "PASS: 22 white API groups, exhaustive palindrome and rational oracles\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
