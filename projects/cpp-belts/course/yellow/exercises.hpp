#pragma once
#include "../white/basics.hpp"
#include "../white/person.hpp"
#include "../white/rational.hpp"
#include <array>
#include <limits>
#include <tuple>
namespace yellow {
using Person = white::Person;
using Rational = white::Rational;
using white::IsPalindrom;
inline int GetDistinctRealRootCount(double a, double b, double c) {
    return static_cast<int>(white::RealRoots(a, b, c).size());
}
class Matrix {
    int rows_ = 0, cols_ = 0;
    std::vector<std::vector<int>> data_;

  public:
    Matrix() = default;
    Matrix(int r, int c) { Reset(r, c); }
    void Reset(int r, int c) {
        if (r < 0 || c < 0)
            throw std::out_of_range("negative matrix extent");
        if (r == 0 || c == 0)
            r = c = 0;
        std::vector<std::vector<int>> replacement(r, std::vector<int>(c));
        data_.swap(replacement);
        rows_ = r;
        cols_ = c;
    }
    int &At(int r, int c) { return data_.at(r).at(c); }
    int At(int r, int c) const { return data_.at(r).at(c); }
    int GetNumRows() const { return rows_; }
    int GetNumColumns() const { return cols_; }
    friend bool operator==(const Matrix &a, const Matrix &b) {
        return a.rows_ == b.rows_ && a.cols_ == b.cols_ && a.data_ == b.data_;
    }
    friend Matrix operator+(const Matrix &a, const Matrix &b) {
        if (a.rows_ != b.rows_ || a.cols_ != b.cols_)
            throw std::invalid_argument("matrix shape mismatch");
        Matrix result(a.rows_, a.cols_);
        for (int i = 0; i < a.rows_; ++i)
            for (int j = 0; j < a.cols_; ++j) {
                const auto v = std::int64_t(a.At(i, j)) + b.At(i, j);
                if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max())
                    throw std::overflow_error("matrix sum exceeds int");
                result.At(i, j) = static_cast<int>(v);
            }
        return result;
    }
    friend std::istream &operator>>(std::istream &in, Matrix &m) {
        int r = 0, c = 0;
        if (!(in >> r >> c))
            return in;
        Matrix result(r, c);
        for (int i = 0; i < r; ++i)
            for (int j = 0; j < c; ++j)
                if (!(in >> result.At(i, j)))
                    return in;
        m = std::move(result);
        return in;
    }
    friend std::ostream &operator<<(std::ostream &out, const Matrix &m) {
        out << m.rows_ << ' ' << m.cols_;
        for (const auto &row : m.data_) {
            out << '\n';
            for (std::size_t j = 0; j < row.size(); ++j) {
                if (j)
                    out << ' ';
                out << row[j];
            }
        }
        return out;
    }
};
enum class Lang { DE, FR, IT };
struct Region {
    std::string std_name, parent_std_name;
    std::map<Lang, std::string> names;
    std::int64_t population;
};
inline bool operator<(const Region &a, const Region &b) {
    return std::tie(a.std_name, a.parent_std_name, a.names, a.population) <
           std::tie(b.std_name, b.parent_std_name, b.names, b.population);
}
inline int FindMaxRepetitionCount(const std::vector<Region> &regions) {
    std::map<Region, int> counts;
    int best = 0;
    for (const auto &r : regions)
        best = std::max(best, ++counts[r]);
    return best;
}
enum class TaskStatus { NEW, IN_PROGRESS, TESTING, DONE };
using TasksInfo = std::map<TaskStatus, int>;
class TeamTasks {
    std::map<std::string, TasksInfo> people_;

  public:
    const TasksInfo &GetPersonTasksInfo(const std::string &p) const { return people_.at(p); }
    void AddNewTask(const std::string &p) { ++people_[p][TaskStatus::NEW]; }
    std::tuple<TasksInfo, TasksInfo> PerformPersonTasks(const std::string &p, int count) {
        if (count < 0)
            throw std::invalid_argument("negative task count");
        TasksInfo updated, untouched;
        auto found = people_.find(p);
        if (found == people_.end())
            return {updated, untouched};
        const auto before = found->second;
        TasksInfo after;
        for (auto [status, n] : before) {
            if (status == TaskStatus::DONE) {
                after[status] += n;
                continue;
            }
            const int moved = std::min(count, n);
            count -= moved;
            if (moved) {
                auto next = static_cast<TaskStatus>(static_cast<int>(status) + 1);
                updated[next] += moved;
                after[next] += moved;
            }
            if (n > moved) {
                untouched[status] = n - moved;
                after[status] += n - moved;
            }
        }
        found->second = std::move(after);
        return {updated, untouched};
    }
};
template <class T> T Sqr(const T &);
template <class T> std::vector<T> Sqr(const std::vector<T> &);
template <class K, class V> std::map<K, V> Sqr(const std::map<K, V> &);
template <class A, class B> std::pair<A, B> Sqr(const std::pair<A, B> &);
template <class T> T Sqr(const T &x) { return x * x; }
template <class T> std::vector<T> Sqr(const std::vector<T> &xs) {
    std::vector<T> r;
    r.reserve(xs.size());
    for (const auto &x : xs)
        r.push_back(Sqr(x));
    return r;
}
template <class K, class V> std::map<K, V> Sqr(const std::map<K, V> &xs) {
    std::map<K, V> r;
    for (const auto &[k, v] : xs)
        r.emplace(k, Sqr(v));
    return r;
}
template <class A, class B> std::pair<A, B> Sqr(const std::pair<A, B> &x) {
    return {Sqr(x.first), Sqr(x.second)};
}
template <class K, class V> V &GetRefStrict(std::map<K, V> &values, const K &key) {
    auto i = values.find(key);
    if (i == values.end())
        throw std::runtime_error("missing key");
    return i->second;
}
inline void PrintVectorPart(const std::vector<int> &values) {
    auto stop = std::find_if(values.begin(), values.end(), [](int v) { return v < 0; });
    bool first = true;
    while (stop != values.begin()) {
        if (!first)
            std::cout << ' ';
        first = false;
        std::cout << *--stop;
    }
}
template <class T> std::vector<T> FindGreaterElements(const std::set<T> &values, const T &border) {
    return {values.upper_bound(border), values.end()};
}
inline std::vector<std::string> SplitIntoWords(const std::string &s) {
    std::vector<std::string> result;
    auto first = s.begin();
    while (first != s.end()) {
        auto last = std::find(first, s.end(), ' ');
        result.emplace_back(first, last);
        first = last == s.end() ? last : std::next(last);
    }
    return result;
}
template <class T> void RemoveDuplicates(std::vector<T> &v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}
template <class It> void MergeSort(It first, It last) {
    if (last - first < 2)
        return;
    std::vector<typename std::iterator_traits<It>::value_type> copy(first, last);
    auto middle = copy.begin() + copy.size() / 2;
    MergeSort(copy.begin(), middle);
    MergeSort(middle, copy.end());
    std::merge(copy.begin(), middle, middle, copy.end(), first);
}
namespace ternary {
template <class It> void MergeSort(It first, It last) {
    const auto n = last - first;
    if (n < 2)
        return;
    using T = typename std::iterator_traits<It>::value_type;
    std::vector<T> copy(first, last), merged;
    merged.reserve(n);
    auto a = copy.begin() + n / 3, b = copy.begin() + 2 * n / 3;
    ternary::MergeSort(copy.begin(), a);
    ternary::MergeSort(a, b);
    ternary::MergeSort(b, copy.end());
    std::merge(copy.begin(), a, a, b, std::back_inserter(merged));
    std::merge(merged.begin(), merged.end(), b, copy.end(), first);
}
} // namespace ternary
inline std::set<int>::const_iterator FindNearestElement(const std::set<int> &values, int border) {
    auto hi = values.lower_bound(border);
    if (hi == values.begin())
        return hi;
    if (hi == values.end())
        return std::prev(hi);
    auto lo = std::prev(hi);
    return std::int64_t(border) - *lo <= std::int64_t(*hi) - border ? lo : hi;
}
template <class It> std::pair<It, It> FindStartsWith(It first, It last, const std::string &prefix) {
    auto lower = std::lower_bound(first, last, prefix);
    auto upper =
        std::upper_bound(lower, last, prefix, [](const std::string &p, const std::string &s) {
            return p < s.substr(0, p.size());
        });
    return {lower, upper};
}
template <class It> std::pair<It, It> FindStartsWith(It first, It last, char prefix) {
    return FindStartsWith(first, last, std::string(1, prefix));
}
namespace demographics {
enum class Gender { FEMALE, MALE };
struct Person {
    int age;
    Gender gender;
    bool is_employed;
};
template <class It> int ComputeMedianAge(It first, It last) {
    if (first == last)
        return 0;
    std::vector<int> ages;
    for (; first != last; ++first)
        ages.push_back(first->age);
    auto middle = ages.begin() + ages.size() / 2;
    std::nth_element(ages.begin(), middle, ages.end());
    return *middle;
}
inline void PrintStats(std::vector<Person> people) {
    auto women = std::partition(people.begin(), people.end(),
                                [](const Person &p) { return p.gender == Gender::FEMALE; });
    auto ew = std::partition(people.begin(), women, [](const Person &p) { return p.is_employed; });
    auto em = std::partition(women, people.end(), [](const Person &p) { return p.is_employed; });
    std::cout << "Median age = " << ComputeMedianAge(people.begin(), people.end())
              << "\nMedian age for females = " << ComputeMedianAge(people.begin(), women)
              << "\nMedian age for males = " << ComputeMedianAge(women, people.end())
              << "\nMedian age for employed females = " << ComputeMedianAge(people.begin(), ew)
              << "\nMedian age for unemployed females = " << ComputeMedianAge(ew, women)
              << "\nMedian age for employed males = " << ComputeMedianAge(women, em)
              << "\nMedian age for unemployed males = " << ComputeMedianAge(em, people.end())
              << '\n';
}
} // namespace demographics
} // namespace yellow
