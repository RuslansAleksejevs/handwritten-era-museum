#pragma once
#include "deque.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iterator>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace museum::red {
template <class It, class Pred> It max_element_if(It first, It last, Pred pred) {
    It best = last;
    for (; first != last; ++first)
        if (pred(*first) && (best == last || *best < *first))
            best = first;
    return best;
}
class Logger {
    std::ostream &out_;
    bool line_ = false, file_ = false;

  public:
    explicit Logger(std::ostream &out) : out_(out) {}
    void SetLogLine(bool value) { line_ = value; }
    void SetLogFile(bool value) { file_ = value; }
    void Log(const std::string &message, const char *file = "", int line = 0) {
        if (file_)
            out_ << file << ':';
        if (line_)
            out_ << line << ':';
        if (file_ || line_)
            out_ << ' ';
        out_ << message << '\n';
    }
};
struct Date {
    int year = 0, month = 0, day = 0;
};
struct Time {
    int hours = 0, minutes = 0;
};
inline bool operator<(Date a, Date b) {
    return std::tie(a.year, a.month, a.day) < std::tie(b.year, b.month, b.day);
}
inline bool operator==(Date a, Date b) {
    return std::tie(a.year, a.month, a.day) == std::tie(b.year, b.month, b.day);
}
inline bool operator<(Time a, Time b) {
    return std::tie(a.hours, a.minutes) < std::tie(b.hours, b.minutes);
}
inline bool operator==(Time a, Time b) {
    return std::tie(a.hours, a.minutes) == std::tie(b.hours, b.minutes);
}
inline std::istream &operator>>(std::istream &in, Date &d) {
    char a, b;
    return in >> d.year >> a >> d.month >> b >> d.day;
}
inline std::istream &operator>>(std::istream &in, Time &t) {
    char c;
    return in >> t.hours >> c >> t.minutes;
}
inline std::ostream &operator<<(std::ostream &o, Date d) {
    return o << d.year << '-' << d.month << '-' << d.day;
}
inline std::ostream &operator<<(std::ostream &o, Time t) {
    return o << t.hours << ':' << t.minutes;
}
struct AirlineTicket {
    std::string from, to, airline;
    Date departure_date;
    Time departure_time;
    Date arrival_date;
    Time arrival_time;
    std::uint64_t price = 0;
};
template <class T>
void UpdateField(T &field, const char *name, const std::map<std::string, std::string> &updates) {
    const auto it = updates.find(name);
    if (it != updates.end()) {
        std::istringstream in(it->second);
        in >> field;
    }
}
struct Student {
    std::string first_name, last_name;
    std::map<std::string, double> marks;
    double rating = 0;
    bool Less(const Student &other) const { return rating > other.rating; }
};
inline bool Compare(const Student &a, const Student &b) { return a.Less(b); }

template <class T> class Table {
    std::vector<std::vector<T>> rows_;
    std::size_t cols_ = 0;

  public:
    Table(std::size_t rows, std::size_t cols) { Resize(rows, cols); }
    void Resize(std::size_t rows, std::size_t cols) {
        rows_.resize(rows);
        for (auto &row : rows_)
            row.resize(cols);
        cols_ = cols;
    }
    auto &operator[](std::size_t row) { return rows_[row]; }
    const auto &operator[](std::size_t row) const { return rows_[row]; }
    std::pair<std::size_t, std::size_t> Size() const { return {rows_.size(), cols_}; }
};
template <class It> class IteratorRange {
    It first_, last_;
    std::size_t count_;

  public:
    IteratorRange(It first, It last)
        : first_(first), last_(last), count_(std::distance(first, last)) {}
    It begin() const { return first_; }
    It end() const { return last_; }
    std::size_t size() const { return count_; }
};
template <class It> class Paginator {
    std::vector<IteratorRange<It>> pages_;

  public:
    Paginator(It first, It last, std::size_t count) {
        if (!count)
            throw std::invalid_argument("page size must be positive");
        while (first != last) {
            It end = first;
            for (std::size_t n = 0; n < count && end != last; ++n)
                ++end;
            pages_.emplace_back(first, end);
            first = end;
        }
    }
    auto begin() const { return pages_.begin(); }
    auto end() const { return pages_.end(); }
    std::size_t size() const { return pages_.size(); }
};
template <class C> auto Paginate(C &c, std::size_t count) {
    return Paginator(std::begin(c), std::end(c), count);
}
class Learner {
    std::set<std::string> words_;

  public:
    int Learn(const std::vector<std::string> &words) {
        int added = 0;
        for (const auto &w : words)
            added += words_.insert(w).second;
        return added;
    }
    std::vector<std::string> KnownWords() const { return {words_.begin(), words_.end()}; }
};

template <class T, std::size_t N> class StackVector {
    std::array<T, N> data_{};
    std::size_t size_;

  public:
    explicit StackVector(std::size_t size = 0) : size_(size) {
        if (size > N)
            throw std::invalid_argument("stack vector size");
    }
    T &operator[](std::size_t i) { return data_[i]; }
    const T &operator[](std::size_t i) const { return data_[i]; }
    auto begin() { return data_.begin(); }
    auto end() { return data_.begin() + size_; }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.begin() + size_; }
    std::size_t Size() const { return size_; }
    std::size_t Capacity() const { return N; }
    void PushBack(const T &value) {
        if (size_ == N)
            throw std::overflow_error("stack vector full");
        data_[size_] = value;
        ++size_;
    }
    T PopBack() {
        if (!size_)
            throw std::underflow_error("stack vector empty");
        return data_[--size_];
    }
};
// Raw storage: only Size() objects are alive. Reallocation moves old elements.
// If an element's move throws, the original vector stays valid but some values may be moved from.
template <class T> class SimpleVector {
    std::allocator<T> alloc_;
    T *data_ = nullptr;
    std::size_t size_ = 0, capacity_ = 0;
    void Destroy() noexcept {
        for (std::size_t i = size_; i > 0; --i)
            std::allocator_traits<decltype(alloc_)>::destroy(alloc_, data_ + i - 1);
        if (data_)
            alloc_.deallocate(data_, capacity_);
    }

  public:
    SimpleVector() = default;
    explicit SimpleVector(std::size_t n) : data_(n ? alloc_.allocate(n) : nullptr), capacity_(n) {
        try {
            for (; size_ < n; ++size_)
                std::allocator_traits<decltype(alloc_)>::construct(alloc_, data_ + size_);
        } catch (...) {
            Destroy();
            throw;
        }
    }
    SimpleVector(const SimpleVector &other)
        : data_(other.size_ ? alloc_.allocate(other.size_) : nullptr), capacity_(other.size_) {
        try {
            for (; size_ < other.size_; ++size_)
                std::allocator_traits<decltype(alloc_)>::construct(alloc_, data_ + size_,
                                                                   other[size_]);
        } catch (...) {
            Destroy();
            throw;
        }
    }
    SimpleVector(SimpleVector &&other) noexcept { Swap(other); }
    SimpleVector &operator=(SimpleVector other) {
        Swap(other);
        return *this;
    }
    ~SimpleVector() { Destroy(); }
    void Swap(SimpleVector &other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }
    T &operator[](std::size_t i) { return data_[i]; }
    const T &operator[](std::size_t i) const { return data_[i]; }
    T *begin() { return data_; }
    T *end() { return size_ ? data_ + size_ : data_; }
    const T *begin() const { return data_; }
    const T *end() const { return size_ ? data_ + size_ : data_; }
    std::size_t Size() const { return size_; }
    std::size_t Capacity() const { return capacity_; }
    void PushBack(T value) {
        if (size_ == capacity_) {
            const auto cap = capacity_ ? capacity_ * 2 : 1;
            if (cap < capacity_)
                throw std::length_error("vector capacity overflow");
            T *fresh = alloc_.allocate(cap);
            std::size_t moved = 0;
            try {
                for (; moved < size_; ++moved)
                    std::allocator_traits<decltype(alloc_)>::construct(alloc_, fresh + moved,
                                                                       std::move(data_[moved]));
                std::allocator_traits<decltype(alloc_)>::construct(alloc_, fresh + size_,
                                                                   std::move(value));
            } catch (...) {
                while (moved)
                    std::allocator_traits<decltype(alloc_)>::destroy(alloc_, fresh + --moved);
                alloc_.deallocate(fresh, cap);
                throw;
            }
            const auto old = size_;
            Destroy();
            data_ = fresh;
            capacity_ = cap;
            size_ = old + 1;
        } else {
            std::allocator_traits<decltype(alloc_)>::construct(alloc_, data_ + size_,
                                                               std::move(value));
            ++size_;
        }
    }
};
} // namespace museum::red
#define LOG(logger, message) (logger).Log((message), __FILE__, __LINE__)
#define SORT_BY(field) [](const auto &left, const auto &right) { return left.field < right.field; }
#define UPDATE_FIELD(ticket, field, values)                                                        \
    ::museum::red::UpdateField((ticket).field, #field, (values))
#define PRINT_VALUES(out, x, y)                                                                    \
    do {                                                                                           \
        auto &museum_output = (out);                                                               \
        museum_output << (x) << std::endl;                                                         \
        museum_output << (y) << std::endl;                                                         \
    } while (false)
#define MUSEUM_JOIN_INNER(a, b) a##b
#define MUSEUM_JOIN(a, b) MUSEUM_JOIN_INNER(a, b)
#define UNIQ_ID MUSEUM_JOIN(museum_unique_, __LINE__)
