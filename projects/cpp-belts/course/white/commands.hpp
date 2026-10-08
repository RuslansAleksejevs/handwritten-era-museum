#pragma once
#include "basics.hpp"
#include "buses.hpp"
#include "database.hpp"
#include "rational.hpp"
#include <cctype>
#include <iomanip>
#include <sstream>

namespace white {
inline void Capitals(std::istream &in, std::ostream &out) {
    int q = 0;
    in >> q;
    std::map<std::string, std::string> capitals;
    while (q--) {
        std::string op, a, b;
        in >> op;
        if (op == "CHANGE_CAPITAL") {
            in >> a >> b;
            const auto it = capitals.find(a);
            if (it == capitals.end())
                out << "Introduce new country " << a << " with capital " << b;
            else if (it->second == b)
                out << "Country " << a << " hasn't changed its capital";
            else
                out << "Country " << a << " has changed its capital from " << it->second << " to "
                    << b;
            capitals[a] = b;
        } else if (op == "RENAME") {
            in >> a >> b;
            if (a == b || !capitals.count(a) || capitals.count(b))
                out << "Incorrect rename, skip";
            else {
                out << "Country " << a << " with capital " << capitals.at(a)
                    << " has been renamed to " << b;
                capitals[b] = capitals.at(a);
                capitals.erase(a);
            }
        } else if (op == "ABOUT") {
            in >> a;
            if (!capitals.count(a))
                out << "Country " << a << " doesn't exist";
            else
                out << "Country " << a << " has capital " << capitals.at(a);
        } else if (op == "DUMP") {
            if (capitals.empty())
                out << "There are no countries in the world";
            bool first = true;
            for (const auto &[country, capital] : capitals) {
                if (!first)
                    out << ' ';
                first = false;
                out << country << '/' << capital;
            }
        } else
            throw std::invalid_argument("unknown capital query");
        out << '\n';
    }
}
inline void Run(const std::string &command, std::istream &in, std::ostream &out) {
    out << std::setprecision(17);
    if (command == "sum") {
        int a = 0, b = 0;
        in >> a >> b;
        out << a + b << '\n';
    } else if (command == "min-string") {
        std::string a, b, c;
        in >> a >> b >> c;
        out << std::min({a, b, c}) << '\n';
    } else if (command == "equation") {
        double a = 0, b = 0, c = 0;
        in >> a >> b >> c;
        PrintWords(RealRoots(a, b, c), out);
        out << '\n';
    } else if (command == "division") {
        int a = 0, b = 0;
        in >> a >> b;
        if (b)
            out << a / b;
        else
            out << "Impossible";
        out << '\n';
    } else if (command == "price") {
        double n = 0, a = 0, b = 0, x = 0, y = 0;
        in >> n >> a >> b >> x >> y;
        out << n * (1 - (n > b ? y : n > a ? x : 0) / 100) << '\n';
    } else if (command == "even") {
        int a = 0, b = 0;
        in >> a >> b;
        std::vector<int> values;
        for (int i = a; i <= b; ++i)
            if (i % 2 == 0)
                values.push_back(i);
        PrintWords(values, out);
        out << '\n';
    } else if (command == "second-f") {
        std::string s;
        std::getline(in, s);
        auto a = s.find('f');
        if (a == std::string::npos)
            out << -2;
        else {
            auto b = s.find('f', a + 1);
            if (b == std::string::npos)
                out << -1;
            else
                out << b;
        }
        out << '\n';
    } else if (command == "gcd") {
        int a = 0, b = 0;
        in >> a >> b;
        out << std::gcd(a, b) << '\n';
    } else if (command == "binary") {
        unsigned n = 0;
        in >> n;
        std::string s;
        do {
            s += char('0' + n % 2);
            n /= 2;
        } while (n);
        std::reverse(s.begin(), s.end());
        out << s << '\n';
    } else if (command == "temperature") {
        int n = 0;
        in >> n;
        std::vector<int> values(n);
        for (auto &x : values)
            in >> x;
        auto indices = AboveAverage(values);
        out << indices.size() << '\n';
        PrintWords(indices, out);
        out << '\n';
    } else if (command == "queue") {
        int q = 0;
        in >> q;
        std::vector<bool> worried;
        std::size_t count = 0;
        while (q--) {
            std::string op;
            in >> op;
            int i = 0;
            if (op == "WORRY_COUNT") {
                out << count << '\n';
                continue;
            }
            in >> i;
            if (op == "COME") {
                if (i < 0) {
                    for (int k = 0; k < -i; ++k) {
                        count -= worried.back();
                        worried.pop_back();
                    }
                } else
                    worried.resize(worried.size() + i, false);
            } else {
                bool value = op == "WORRY";
                if (worried.at(i) != value) {
                    if (value)
                        ++count;
                    else
                        --count;
                    worried[i] = value;
                }
            }
        }
    } else if (command == "months") {
        int q = 0;
        in >> q;
        MonthTasks tasks;
        while (q--) {
            std::string op, s;
            int day = 0;
            in >> op;
            if (op == "NEXT")
                tasks.Next();
            else {
                in >> day;
                if (op == "ADD") {
                    in >> s;
                    tasks.Add(day, s);
                } else {
                    const auto &values = tasks.Get(day);
                    out << values.size();
                    for (const auto &x : values)
                        out << ' ' << x;
                    out << '\n';
                }
            }
        }
    } else if (command == "anagrams") {
        int n = 0;
        in >> n;
        while (n--) {
            std::string a, b;
            in >> a >> b;
            out << (BuildCharCounters(a) == BuildCharCounters(b) ? "YES" : "NO") << '\n';
        }
    } else if (command == "capitals")
        Capitals(in, out);
    else if (command == "buses")
        RunBuses(in, out);
    else if (command == "routes" || command == "route-sets") {
        int q = 0;
        in >> q;
        std::map<std::vector<std::string>, int> routes;
        while (q--) {
            int n = 0;
            in >> n;
            std::vector<std::string> route(n);
            for (auto &s : route)
                in >> s;
            if (command == "route-sets") {
                std::sort(route.begin(), route.end());
                route.erase(std::unique(route.begin(), route.end()), route.end());
            }
            auto it = routes.find(route);
            if (it != routes.end())
                out << "Already exists for " << it->second;
            else {
                int id = static_cast<int>(routes.size()) + 1;
                routes.emplace(route, id);
                out << "New bus " << id;
            }
            out << '\n';
        }
    } else if (command == "unique") {
        int n = 0;
        in >> n;
        std::set<std::string> words;
        while (n--) {
            std::string s;
            in >> s;
            words.insert(s);
        }
        out << words.size() << '\n';
    } else if (command == "synonyms") {
        int q = 0;
        in >> q;
        std::map<std::string, std::set<std::string>> words;
        while (q--) {
            std::string op, a, b;
            in >> op >> a;
            if (op == "COUNT")
                out << (words.count(a) ? words.at(a).size() : 0) << '\n';
            else {
                in >> b;
                if (op == "ADD") {
                    words[a].insert(b);
                    words[b].insert(a);
                } else
                    out << (words.count(a) && words.at(a).count(b) ? "YES" : "NO") << '\n';
            }
        }
    } else if (command == "abs-sort") {
        int n = 0;
        in >> n;
        std::vector<int> v(n);
        for (auto &x : v)
            in >> x;
        std::stable_sort(v.begin(), v.end(), [](int a, int b) {
            return std::abs(std::int64_t(a)) < std::abs(std::int64_t(b));
        });
        PrintWords(v, out);
        out << '\n';
    } else if (command == "case-sort") {
        int n = 0;
        in >> n;
        std::vector<std::string> v(n);
        for (auto &x : v)
            in >> x;
        const auto lower = [](std::string s) {
            for (char &c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        std::stable_sort(v.begin(), v.end(),
                         [&](const auto &a, const auto &b) { return lower(a) < lower(b); });
        PrintWords(v, out);
        out << '\n';
    } else if (command == "copy" || command == "copy-file")
        out << in.rdbuf();
    else if (command == "precision") {
        out << std::fixed << std::setprecision(3);
        double x;
        while (in >> x)
            out << x << '\n';
    } else if (command == "table") {
        int n = 0, m = 0;
        in >> n >> m;
        for (int i = 0; i < n; ++i) {
            if (i)
                out << '\n';
            for (int j = 0; j < m; ++j) {
                int x = 0;
                char comma;
                if (j) {
                    in >> comma;
                    if (comma != ',')
                        throw std::invalid_argument("missing comma");
                    out << ' ';
                }
                in >> x;
                out << std::setw(10) << x;
            }
        }
    } else if (command == "students") {
        struct Student {
            std::string first, last;
            int day = 0, month = 0, year = 0;
        };
        int n = 0;
        in >> n;
        std::vector<Student> v(n);
        for (auto &s : v)
            in >> s.first >> s.last >> s.day >> s.month >> s.year;
        int q = 0;
        in >> q;
        while (q--) {
            std::string op;
            int k = 0;
            in >> op >> k;
            if (k < 1 || k > n || (op != "name" && op != "date"))
                out << "bad request";
            else {
                const auto &s = v[k - 1];
                if (op == "name")
                    out << s.first << ' ' << s.last;
                else
                    out << s.day << '.' << s.month << '.' << s.year;
            }
            out << '\n';
        }
    } else if (command == "calculator") {
        try {
            Rational a, b;
            char op;
            if (!(in >> a >> op >> b))
                throw std::invalid_argument("bad rational input");
            if (op == '+')
                out << a + b;
            else if (op == '-')
                out << a - b;
            else if (op == '*')
                out << a * b;
            else if (op == '/')
                out << a / b;
            else
                throw std::invalid_argument("bad operation");
        } catch (const std::invalid_argument &) {
            out << "Invalid argument";
        } catch (const std::domain_error &) {
            out << "Division by zero";
        }
        out << '\n';
    } else if (command == "database")
        RunDatabase(in, out);
    else
        throw std::invalid_argument("unknown exercise: " + command);
}
} // namespace white
