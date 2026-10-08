#pragma once
#include <stdexcept>
#include <string>
#include <vector>
namespace yellow::contracts {
inline void Require(bool value) {
    if (!value)
        throw std::runtime_error("candidate violated exercise contract");
}
template <class F> void RootCounter(F f) {
    for (auto values : std::vector<std::vector<double>>{{1, 0, -1, 2},
                                                        {1, 2, 1, 1},
                                                        {1, 0, 1, 0},
                                                        {0, 2, 3, 1},
                                                        {0, 0, 7, 0},
                                                        {0, 1, 0, 1},
                                                        {1, 0, 0, 1},
                                                        {-1, 0, 1, 2},
                                                        {-1, 2, -1, 1}})
        Require(f(values[0], values[1], values[2]) == static_cast<int>(values[3]));
}
template <class P> void Person() {
    P p;
    Require(p.GetFullName(100) == "Incognito");
    p.ChangeFirstName(20, "Ada");
    Require(p.GetFullName(20) == "Ada with unknown last name");
    p.ChangeLastName(10, "Green");
    Require(p.GetFullName(10) == "Green with unknown first name");
    Require(p.GetFullName(19) == "Green with unknown first name");
    Require(p.GetFullName(21) == "Ada Green");
    p.ChangeFirstName(5, "Ann");
    p.ChangeLastName(30, "Brown");
    Require(p.GetFullName(6) == "Ann with unknown last name");
    Require(p.GetFullName(29) == "Ada Green");
    Require(p.GetFullName(30) == "Ada Brown");
}
template <class R> void Rational() {
    R zero;
    Require(zero.Numerator() == 0 && zero.Denominator() == 1);
    for (int n = -12; n <= 12; ++n)
        for (int d = -12; d <= 12; ++d)
            if (d) {
                R r(n, d);
                Require(r.Denominator() > 0);
                Require(r.Numerator() * d == n * r.Denominator());
                if (n == 0)
                    Require(r.Denominator() == 1);
                for (int factor = 2; factor <= 24; ++factor)
                    Require(r.Numerator() % factor != 0 || r.Denominator() % factor != 0);
            }
}
template <class F> void Palindrome(F f) {
    for (int n = 0; n <= 8; ++n)
        for (int bits = 0; bits < (1 << n); ++bits) {
            std::string s;
            for (int i = 0; i < n; ++i)
                s += (bits >> i & 1) ? 'x' : ' ';
            Require(f(s) == (s == std::string(s.rbegin(), s.rend())));
        }
    Require(!f("Aa"));
    Require(f("z"));
    Require(!f("abccaa"));
}
} // namespace yellow::contracts
