#pragma once
#include "../red/algorithms.hpp"
#include "../red/concurrency.hpp"
#include <atomic>
#include <forward_list>
#include <optional>
#include <unordered_map>
namespace museum::brown {
using museum::red::ObjectPool;
using museum::red::PriorityCollection;
using museum::red::Synchronized;
template <class T, class Hasher = std::hash<T>> class HashSet {
    std::vector<std::forward_list<T>> buckets_;
    Hasher hasher_;

  public:
    using BucketList = std::forward_list<T>;
    explicit HashSet(std::size_t count, const Hasher &hash = {}) : buckets_(count), hasher_(hash) {
        if (!count)
            throw std::invalid_argument("zero hash buckets");
    }
    const BucketList &GetBucket(const T &value) const {
        return buckets_[hasher_(value) % buckets_.size()];
    }
    bool Has(const T &value) const {
        const auto &b = GetBucket(value);
        return std::find(b.begin(), b.end(), value) != b.end();
    }
    void Add(const T &value) {
        if (!Has(value))
            buckets_[hasher_(value) % buckets_.size()].push_front(value);
    }
    void Erase(const T &value) { buckets_[hasher_(value) % buckets_.size()].remove(value); }
};
struct Node {
    int value;
    Node *left = nullptr;
    Node *right = nullptr;
    Node *parent = nullptr;
};
inline Node *Next(Node *node) {
    if (!node)
        return nullptr;
    if (node->right) {
        node = node->right;
        while (node->left)
            node = node->left;
        return node;
    }
    while (node->parent && node == node->parent->right)
        node = node->parent;
    return node->parent;
}
struct Point3D {
    int x, y, z;
    bool operator==(const Point3D &b) const { return x == b.x && y == b.y && z == b.z; }
};
inline std::size_t Combine(std::size_t left, std::size_t right) { return left * 1000003u + right; }
struct Hasher {
    std::size_t operator()(Point3D p) const {
        return Combine(Combine(std::hash<int>{}(p.x), std::hash<int>{}(p.y)),
                       std::hash<int>{}(p.z));
    }
};
struct Address {
    std::string city, street;
    int building;
    bool operator==(const Address &b) const {
        return std::tie(city, street, building) == std::tie(b.city, b.street, b.building);
    }
};
struct Person {
    std::string name;
    int height;
    double weight;
    Address address;
    bool operator==(const Person &b) const {
        return name == b.name && height == b.height && weight == b.weight && address == b.address;
    }
};
struct AddressHasher {
    std::size_t operator()(const Address &a) const {
        return Combine(
            Combine(std::hash<std::string>{}(a.city), std::hash<std::string>{}(a.street)),
            std::hash<int>{}(a.building));
    }
};
struct PersonHasher {
    std::size_t operator()(const Person &p) const {
        return Combine(
            Combine(Combine(std::hash<std::string>{}(p.name), std::hash<int>{}(p.height)),
                    std::hash<double>{}(p.weight)),
            AddressHasher{}(p.address));
    }
};
struct Record {
    std::string id, title, user;
    int timestamp, karma;
};
class Database {
    using IntIndex = std::multimap<int, const Record *>;
    using UserIndex = std::multimap<std::string, const Record *>;
    struct Entry {
        Record record;
        IntIndex::iterator timestamp, karma;
        UserIndex::iterator user;
    };
    std::map<std::string, Entry> records_;
    IntIndex timestamps_, karmas_;
    UserIndex users_;
    template <class Index, class Key, class Callback>
    static void Range(const Index &index, const Key &low, const Key &high, Callback callback) {
        if (high < low)
            return;
        for (auto it = index.lower_bound(low), end = index.upper_bound(high); it != end; ++it)
            if (!callback(*it->second))
                break;
    }

  public:
    Database() = default;
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    bool Put(const Record &record) {
        auto [it, added] = records_.try_emplace(record.id, Entry{record, {}, {}, {}});
        if (!added)
            return false;
        auto &entry = it->second;
        entry.timestamp = timestamps_.emplace(record.timestamp, &entry.record);
        entry.karma = karmas_.emplace(record.karma, &entry.record);
        entry.user = users_.emplace(record.user, &entry.record);
        return true;
    }
    const Record *GetById(const std::string &id) const {
        auto it = records_.find(id);
        return it == records_.end() ? nullptr : &it->second.record;
    }
    bool Erase(const std::string &id) {
        auto it = records_.find(id);
        if (it == records_.end())
            return false;
        timestamps_.erase(it->second.timestamp);
        karmas_.erase(it->second.karma);
        users_.erase(it->second.user);
        records_.erase(it);
        return true;
    }
    template <class Callback> void RangeByTimestamp(int low, int high, Callback callback) const {
        Range(timestamps_, low, high, callback);
    }
    template <class Callback> void RangeByKarma(int low, int high, Callback callback) const {
        Range(karmas_, low, high, callback);
    }
    template <class Callback> void AllByUser(const std::string &user, Callback callback) const {
        Range(users_, user, user, callback);
    }
};
template <class T> class LazyValue {
    std::function<T()> initialize_;
    mutable std::optional<T> value_;
    mutable std::once_flag initialized_;
    mutable std::atomic<bool> ready_{false};

  public:
    explicit LazyValue(std::function<T()> f) : initialize_(std::move(f)) {}
    const T &Get() const {
        std::call_once(initialized_, [this] {
            value_.emplace(initialize_());
            ready_.store(true, std::memory_order_release);
        });
        return *value_;
    }
    bool HasValue() const { return ready_.load(std::memory_order_acquire); }
};
template <class K, class V, class Hash = std::hash<K>> class ConcurrentMap {
  public:
    using MapType = std::unordered_map<K, V, Hash>;

  private:
    struct Bucket {
        mutable std::mutex mutex;
        MapType values;
    };
    std::vector<Bucket> buckets_;
    Hash hash_;
    Bucket &BucketFor(const K &key) { return buckets_[hash_(key) % buckets_.size()]; }
    const Bucket &BucketFor(const K &key) const { return buckets_[hash_(key) % buckets_.size()]; }

  public:
    struct WriteAccess {
        std::unique_lock<std::mutex> guard;
        V &ref_to_value;
        WriteAccess(Bucket &b, const K &k) : guard(b.mutex), ref_to_value(b.values[k]) {}
    };
    struct ReadAccess {
        std::unique_lock<std::mutex> guard;
        const V &ref_to_value;
        ReadAccess(const Bucket &b, const K &k) : guard(b.mutex), ref_to_value(b.values.at(k)) {}
    };
    explicit ConcurrentMap(std::size_t count) : buckets_(count) {
        if (!count)
            throw std::invalid_argument("zero concurrent buckets");
    }
    WriteAccess operator[](const K &key) { return {BucketFor(key), key}; }
    ReadAccess At(const K &key) const { return {BucketFor(key), key}; }
    bool Has(const K &key) const {
        const auto &b = BucketFor(key);
        std::lock_guard<std::mutex> lock(b.mutex);
        return b.values.count(key);
    }
    MapType BuildOrdinaryMap() const {
        MapType result;
        for (const auto &b : buckets_) {
            std::lock_guard<std::mutex> lock(b.mutex);
            result.insert(b.values.begin(), b.values.end());
        }
        return result;
    }
};
template <class T> class Polynomial {
    std::vector<T> coefficients_;
    void Trim() {
        while (!coefficients_.empty() && coefficients_.back() == T{})
            coefficients_.pop_back();
    }
    void Set(std::size_t degree, T value) {
        if (degree >= coefficients_.size()) {
            if (value == T{})
                return;
            coefficients_.resize(degree + 1);
        }
        coefficients_[degree] = std::move(value);
        if (degree + 1 == coefficients_.size())
            Trim();
    }

  public:
    Polynomial() = default;
    Polynomial(std::vector<T> values) : coefficients_(std::move(values)) { Trim(); }
    template <class It> Polynomial(It first, It last) : coefficients_(first, last) { Trim(); }
    class Proxy {
        Polynomial &owner_;
        std::size_t index_;

      public:
        Proxy(Polynomial &owner, std::size_t i) : owner_(owner), index_(i) {}
        operator T() const { return static_cast<const Polynomial &>(owner_)[index_]; }
        Proxy &operator=(T value) {
            owner_.Set(index_, std::move(value));
            return *this;
        }
        Proxy &operator=(const Proxy &other) { return *this = static_cast<T>(other); }
    };
    T operator[](std::size_t i) const { return i < coefficients_.size() ? coefficients_[i] : T{}; }
    Proxy operator[](std::size_t i) { return {*this, i}; }
    int Degree() const { return static_cast<int>(coefficients_.size()) - 1; }
    bool operator==(const Polynomial &other) const { return coefficients_ == other.coefficients_; }
    bool operator!=(const Polynomial &other) const { return !(*this == other); }
    Polynomial &operator+=(const Polynomial &other) {
        coefficients_.resize(std::max(coefficients_.size(), other.coefficients_.size()));
        for (std::size_t i = 0; i < other.coefficients_.size(); ++i)
            coefficients_[i] += other.coefficients_[i];
        Trim();
        return *this;
    }
    Polynomial &operator-=(const Polynomial &other) {
        coefficients_.resize(std::max(coefficients_.size(), other.coefficients_.size()));
        for (std::size_t i = 0; i < other.coefficients_.size(); ++i)
            coefficients_[i] -= other.coefficients_[i];
        Trim();
        return *this;
    }
    T operator()(const T &x) const {
        T result{};
        for (auto it = coefficients_.rbegin(); it != coefficients_.rend(); ++it)
            result = result * x + *it;
        return result;
    }
    using const_iterator = typename std::vector<T>::const_iterator;
    const_iterator begin() const { return coefficients_.begin(); }
    const_iterator end() const { return coefficients_.end(); }
};
} // namespace museum::brown
