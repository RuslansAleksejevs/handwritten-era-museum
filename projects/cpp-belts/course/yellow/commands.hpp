#pragma once
#include "budget.hpp"
#include "buses/bus_manager.h"
#include "events/database.h"
#include "exercises.hpp"
#include "oop.hpp"
#include <deque>
#include <iomanip>
namespace yellow {
inline void Run(const std::string &name, std::istream &in, std::ostream &out) {
    if (name == "matrix") {
        Matrix a, b;
        in >> a >> b;
        out << a + b << '\n';
    } else if (name == "temperature") {
        int n = 0;
        in >> n;
        std::vector<int> v(n);
        for (auto &x : v)
            in >> x;
        auto indices = white::AboveAverage(v);
        out << indices.size() << '\n';
        for (std::size_t i = 0; i < indices.size(); ++i) {
            if (i)
                out << ' ';
            out << indices[i];
        }
        out << '\n';
    } else if (name == "blocks") {
        std::uint64_t n = 0, density = 0, total = 0;
        in >> n >> density;
        while (n--) {
            std::uint64_t w = 0, h = 0, d = 0;
            in >> w >> h >> d;
            total += w * h * d * density;
        }
        out << total << '\n';
    } else if (name == "buses")
        buses::Run(in, out);
    else if (name == "database")
        events::Run(in, out);
    else if (name == "permutations") {
        int n = 0;
        in >> n;
        std::vector<int> v(n);
        std::iota(v.rbegin(), v.rend(), 1);
        do {
            for (int i = 0; i < n; ++i) {
                if (i)
                    out << ' ';
                out << v[i];
            }
            out << '\n';
        } while (std::prev_permutation(v.begin(), v.end()));
    } else if (name == "budget") {
        Budget b;
        int n = 0;
        in >> n;
        out << std::setprecision(25);
        while (n--) {
            std::string op, a, z;
            in >> op >> a >> z;
            auto first = white::ParseDate(a), last = white::ParseDate(z);
            if (op == "Earn") {
                long double value = 0;
                in >> value;
                b.Earn(first, last, value);
            } else
                out << b.ComputeIncome(first, last) << '\n';
        }
    } else if (name == "budget-prefix") {
        PrefixBudget b;
        int n = 0;
        in >> n;
        while (n--) {
            std::string s;
            std::int64_t value = 0;
            in >> s >> value;
            b.Earn(white::ParseDate(s), value);
        }
        b.Seal();
        in >> n;
        while (n--) {
            std::string a, z;
            in >> a >> z;
            out << b.ComputeIncome(white::ParseDate(a), white::ParseDate(z)) << '\n';
        }
    } else if (name == "arithmetic" || name == "arithmetic-minimal") {
        int x = 0, n = 0;
        in >> x >> n;
        std::deque<std::string> parts{std::to_string(x)};
        char previous = '*';
        for (int i = 0; i < n; ++i) {
            char op;
            int value = 0;
            in >> op >> value;
            if (name == "arithmetic" ||
                ((op == '*' || op == '/') && (previous == '+' || previous == '-'))) {
                parts.push_front("(");
                parts.push_back(")");
            }
            parts.push_back(" " + std::string(1, op) + " " + std::to_string(value));
            previous = op;
        }
        for (const auto &s : parts)
            out << s;
        out << '\n';
    } else if (name == "figures") {
        std::vector<std::shared_ptr<Figure>> figures;
        std::string command;
        out << std::fixed << std::setprecision(3);
        while (in >> command) {
            if (command == "ADD")
                figures.push_back(CreateFigure(in));
            else if (command == "PRINT")
                for (const auto &f : figures)
                    out << f->Name() << ' ' << f->Perimeter() << ' ' << f->Area() << '\n';
            else
                throw std::invalid_argument("unknown figure command");
        }
    } else
        throw std::invalid_argument("unknown exercise: " + name);
}
} // namespace yellow
