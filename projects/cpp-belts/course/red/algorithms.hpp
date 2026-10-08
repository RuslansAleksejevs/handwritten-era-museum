#pragma once
#include "foundations.hpp"
#include <list>
#include <queue>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace museum::red {
template <class T> class ObjectPool {
    std::unordered_map<T *, std::unique_ptr<T>> owned_;
    std::unordered_set<T *> allocated_;
    std::queue<T *> free_;

  public:
    T *TryAllocate() {
        if (free_.empty())
            return nullptr;
        T *p = free_.front();
        allocated_.insert(p);
        free_.pop();
        return p;
    }
    T *Allocate() {
        if (T *p = TryAllocate())
            return p;
        auto object = std::make_unique<T>();
        T *p = object.get();
        owned_.emplace(p, std::move(object));
        try {
            allocated_.insert(p);
        } catch (...) {
            owned_.erase(p);
            throw;
        }
        return p;
    }
    void Deallocate(T *p) {
        auto it = allocated_.find(p);
        if (it == allocated_.end())
            throw std::invalid_argument("object is not allocated");
        free_.push(p);
        allocated_.erase(it);
    }
};
template <class T> void Swap(T *a, T *b) {
    using std::swap;
    swap(*a, *b);
}
template <class T> void SortPointers(std::vector<T *> &values) {
    std::sort(values.begin(), values.end(), [](T *a, T *b) { return *a < *b; });
}
// Copy the nonoverlapping source tails once; reverse the shared middle in place.
// std::less provides a total order even for pointers into different arrays.
template <class T> void ReversedCopy(T *src, std::size_t count, T *dst) {
    if (!count)
        return;
    const auto less = std::less<T *>{};
    if (!less(dst, src + count) || !less(src, dst + count)) {
        for (std::size_t i = 0; i < count; ++i)
            dst[i] = src[count - 1 - i];
        return;
    }
    T *left = less(src, dst) ? dst : src;
    T *right = less(src + count, dst + count) ? src + count : dst + count;
    const auto overlap = static_cast<std::size_t>(right - left);
    if (less(dst, src)) {
        for (std::size_t i = 0; i < count - overlap; ++i)
            dst[i] = src[count - 1 - i];
    } else {
        for (std::size_t i = overlap; i < count; ++i)
            dst[i] = src[count - 1 - i];
    }
    std::reverse(left, right);
}
template <class T> class LinkedList {
  public:
    struct Node {
        T value;
        Node *next = nullptr;
    };

  private:
    Node *head_ = nullptr;

  public:
    LinkedList() = default;
    LinkedList(const LinkedList &) = delete;
    LinkedList &operator=(const LinkedList &) = delete;
    ~LinkedList() {
        while (head_)
            PopFront();
    }
    void PushFront(const T &value) { head_ = new Node{value, head_}; }
    void InsertAfter(Node *p, const T &value) {
        if (!p)
            PushFront(value);
        else
            p->next = new Node{value, p->next};
    }
    void PopFront() {
        if (head_) {
            auto p = head_;
            head_ = p->next;
            delete p;
        }
    }
    void RemoveAfter(Node *p) {
        if (!p)
            PopFront();
        else if (p->next) {
            auto q = p->next;
            p->next = q->next;
            delete q;
        }
    }
    Node *GetHead() { return head_; }
    const Node *GetHead() const { return head_; }
};
class Translator {
    std::set<std::string, std::less<>> storage_;
    std::map<std::string_view, std::string_view> forward_, backward_;
    std::string_view Intern(std::string_view s) {
        auto it = storage_.find(s);
        if (it == storage_.end())
            it = storage_.emplace(s).first;
        return *it;
    }

  public:
    Translator() = default;
    Translator(const Translator &) = delete;
    Translator &operator=(const Translator &) = delete;
    void Add(std::string_view source, std::string_view target) {
        auto s = Intern(source), t = Intern(target);
        forward_[s] = t;
        backward_[t] = s;
    }
    std::string_view TranslateForward(std::string_view s) const {
        auto it = forward_.find(s);
        return it == forward_.end() ? std::string_view{} : it->second;
    }
    std::string_view TranslateBackward(std::string_view s) const {
        auto it = backward_.find(s);
        return it == backward_.end() ? std::string_view{} : it->second;
    }
};
template <class Airport> class AirportCounter {
    static constexpr std::size_t N = static_cast<std::size_t>(Airport::Last_);
    std::array<std::size_t, N> counts_{};

  public:
    using Item = std::pair<Airport, std::size_t>;
    AirportCounter() = default;
    template <class It> AirportCounter(It first, It last) {
        for (; first != last; ++first)
            Insert(*first);
    }
    std::size_t Get(Airport a) const { return counts_.at(static_cast<std::size_t>(a)); }
    void Insert(Airport a) { ++counts_.at(static_cast<std::size_t>(a)); }
    void EraseOne(Airport a) {
        auto &n = counts_.at(static_cast<std::size_t>(a));
        if (n)
            --n;
    }
    void EraseAll(Airport a) { counts_.at(static_cast<std::size_t>(a)) = 0; }
    auto GetItems() const {
        std::array<Item, N> out{};
        for (std::size_t i = 0; i < N; ++i)
            out[i] = {static_cast<Airport>(i), counts_[i]};
        return out;
    }
};
class Editor {
    std::list<char> text_;
    std::list<char>::iterator cursor_ = text_.end();
    std::string clipboard_;

  public:
    Editor() = default;
    Editor(const Editor &) = delete;
    Editor &operator=(const Editor &) = delete;
    void Left() {
        if (cursor_ != text_.begin())
            --cursor_;
    }
    void Right() {
        if (cursor_ != text_.end())
            ++cursor_;
    }
    void Insert(char c) { text_.insert(cursor_, c); }
    void Copy(std::size_t count) {
        clipboard_.clear();
        auto it = cursor_;
        while (count-- && it != text_.end())
            clipboard_ += *it++;
    }
    void Cut(std::size_t count) {
        Copy(count);
        for (std::size_t i = 0; i < clipboard_.size(); ++i)
            cursor_ = text_.erase(cursor_);
    }
    void Paste() { text_.insert(cursor_, clipboard_.begin(), clipboard_.end()); }
    std::string GetText() const { return {text_.begin(), text_.end()}; }
};
namespace web {
struct HttpRequest {
    std::string_view method, uri, protocol;
};
inline HttpRequest ParseRequest(std::string_view line) {
    auto next = [&] {
        auto start = line.find_first_not_of(' ');
        if (start == line.npos) {
            line = {};
            return std::string_view{};
        }
        line.remove_prefix(start);
        auto end = line.find(' ');
        auto result = line.substr(0, end);
        if (end == line.npos)
            line = {};
        else
            line.remove_prefix(end);
        return result;
    };
    HttpRequest out;
    out.method = next();
    out.uri = next();
    out.protocol = next();
    return out;
}
class Stats {
    std::map<std::string_view, int> methods_{
        {"GET", 0}, {"POST", 0}, {"PUT", 0}, {"DELETE", 0}, {"UNKNOWN", 0}},
        uris_{{"/", 0},       {"/order", 0}, {"/product", 0},
              {"/basket", 0}, {"/help", 0},  {"unknown", 0}};

  public:
    void AddMethod(std::string_view value) {
        auto it = methods_.find(value);
        ++(it == methods_.end() ? methods_.at("UNKNOWN") : it->second);
    }
    void AddUri(std::string_view value) {
        auto it = uris_.find(value);
        ++(it == uris_.end() ? uris_.at("unknown") : it->second);
    }
    const auto &GetMethodStats() const { return methods_; }
    const auto &GetUriStats() const { return uris_; }
};
} // namespace web
template <class RandomIt>
void MakeJosephusPermutation(RandomIt first, RandomIt last, std::uint32_t step) {
    if (!step)
        throw std::invalid_argument("Josephus step is zero");
    using T = typename std::iterator_traits<RandomIt>::value_type;
    std::list<T> pool(std::make_move_iterator(first), std::make_move_iterator(last));
    auto it = pool.begin();
    while (!pool.empty()) {
        *first++ = std::move(*it);
        it = pool.erase(it);
        if (pool.empty())
            break;
        if (it == pool.end())
            it = pool.begin();
        for (std::size_t k = 0; k < (step - 1) % pool.size(); ++k)
            if (++it == pool.end())
                it = pool.begin();
    }
}
template <class String> using Group = std::vector<String>;
template <class String> std::vector<Group<String>> GroupHeavyStrings(std::vector<String> strings) {
    using Char = typename String::value_type;
    std::map<std::vector<Char>, std::size_t> indexes;
    std::vector<Group<String>> groups;
    for (auto &word : strings) {
        std::vector<Char> key(word.begin(), word.end());
        std::sort(key.begin(), key.end());
        key.erase(std::unique(key.begin(), key.end()), key.end());
        auto [it, added] = indexes.emplace(std::move(key), groups.size());
        if (added)
            groups.emplace_back();
        groups[it->second].push_back(std::move(word));
    }
    return groups;
}
template <class Token> using Sentence = std::vector<Token>;
template <class Token> std::vector<Sentence<Token>> SplitIntoSentences(std::vector<Token> tokens) {
    std::vector<Sentence<Token>> out;
    bool punctuation = false;
    for (auto &token : tokens) {
        bool current = token.IsEndSentencePunctuation();
        if (out.empty() || (punctuation && !current))
            out.emplace_back();
        out.back().push_back(std::move(token));
        punctuation = current;
    }
    return out;
}
template <class It> void MergeSort(It first, It last) {
    const auto n = last - first;
    if (n < 2)
        return;
    if (n % 3)
        throw std::invalid_argument("three-way merge sort requires a power of three");
    using T = typename std::iterator_traits<It>::value_type;
    std::vector<T> data(std::make_move_iterator(first), std::make_move_iterator(last)), temp;
    temp.reserve(n * 2 / 3);
    auto a = data.begin() + n / 3, b = a + n / 3;
    MergeSort(data.begin(), a);
    MergeSort(a, b);
    MergeSort(b, data.end());
    std::merge(std::make_move_iterator(data.begin()), std::make_move_iterator(a),
               std::make_move_iterator(a), std::make_move_iterator(b), std::back_inserter(temp));
    std::merge(std::make_move_iterator(temp.begin()), std::make_move_iterator(temp.end()),
               std::make_move_iterator(b), std::make_move_iterator(data.end()), first);
}
template <class T> class PriorityCollection {
  public:
    using Id = std::size_t;

  private:
    struct Item {
        T object;
        int priority = 0;
    };
    std::map<Id, Item> objects_;
    std::set<std::pair<int, Id>> order_;
    Id next_ = 0;

  public:
    Id Add(T value) {
        const Id id = next_++;
        objects_.emplace(id, Item{std::move(value)});
        try {
            order_.emplace(0, id);
        } catch (...) {
            objects_.erase(id);
            throw;
        }
        return id;
    }
    template <class It, class Out> void Add(It first, It last, Out out) {
        for (; first != last; ++first)
            *out++ = Add(std::move(*first));
    }
    bool IsValid(Id id) const { return objects_.count(id); }
    const T &Get(Id id) const { return objects_.at(id).object; }
    void Promote(Id id) {
        auto &value = objects_.at(id);
        order_.emplace(value.priority + 1, id);
        order_.erase({value.priority, id});
        ++value.priority;
    }
    std::pair<const T &, int> GetMax() const {
        auto [p, id] = *order_.rbegin();
        return {Get(id), p};
    }
    std::pair<T, int> PopMax() {
        auto [p, id] = *order_.rbegin();
        T value = std::move(objects_.at(id).object);
        order_.erase({p, id});
        objects_.erase(id);
        return {std::move(value), p};
    }
};
} // namespace museum::red
