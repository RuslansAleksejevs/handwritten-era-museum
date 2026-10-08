#pragma once
#include <cstdint>
#include <istream>
#include <limits>
#include <numeric>
#include <ostream>
#include <stdexcept>
namespace white {
class Rational {
    int n_ = 0, d_ = 1;
    static Rational FromWide(std::int64_t n, std::int64_t d) {
        if (d == 0)
            throw std::invalid_argument("zero denominator");
        const auto g = std::gcd(n, d);
        n /= g;
        d /= g;
        if (d < 0) {
            n = -n;
            d = -d;
        }
        if (n < std::numeric_limits<int>::min() || n > std::numeric_limits<int>::max() ||
            d > std::numeric_limits<int>::max())
            throw std::overflow_error("normalized rational exceeds int");
        Rational r;
        r.n_ = static_cast<int>(n);
        r.d_ = static_cast<int>(d);
        return r;
    }

  public:
    Rational() = default;
    Rational(int n, int d) { *this = FromWide(n, d); }
    int Numerator() const { return n_; }
    int Denominator() const { return d_; }
    friend bool operator==(const Rational &a, const Rational &b) {
        return a.n_ == b.n_ && a.d_ == b.d_;
    }
    friend bool operator<(const Rational &a, const Rational &b) {
        return std::int64_t(a.n_) * b.d_ < std::int64_t(b.n_) * a.d_;
    }
    friend Rational operator+(const Rational &a, const Rational &b) {
        return FromWide(std::int64_t(a.n_) * b.d_ + std::int64_t(b.n_) * a.d_,
                        std::int64_t(a.d_) * b.d_);
    }
    friend Rational operator-(const Rational &a, const Rational &b) {
        return FromWide(std::int64_t(a.n_) * b.d_ - std::int64_t(b.n_) * a.d_,
                        std::int64_t(a.d_) * b.d_);
    }
    friend Rational operator*(const Rational &a, const Rational &b) {
        return FromWide(std::int64_t(a.n_) * b.n_, std::int64_t(a.d_) * b.d_);
    }
    friend Rational operator/(const Rational &a, const Rational &b) {
        if (b.n_ == 0)
            throw std::domain_error("division by zero");
        return FromWide(std::int64_t(a.n_) * b.d_, std::int64_t(a.d_) * b.n_);
    }
    friend std::ostream &operator<<(std::ostream &out, const Rational &r) {
        return out << r.n_ << '/' << r.d_;
    }
    friend std::istream &operator>>(std::istream &in, Rational &r) {
        int n, d;
        char slash;
        if (in >> n >> slash >> d) {
            if (slash != '/')
                in.setstate(std::ios::failbit);
            else
                r = Rational(n, d);
        }
        return in;
    }
};
} // namespace white
