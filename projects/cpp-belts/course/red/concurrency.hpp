#pragma once
#include "foundations.hpp"
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>
namespace museum::red {
template <class T> class Synchronized {
    T value_;
    mutable std::mutex mutex_;

  public:
    explicit Synchronized(T value = T{}) : value_(std::move(value)) {}
    struct Access {
        std::unique_lock<std::mutex> guard;
        T &ref_to_value;
        Access(std::mutex &m, T &v) : guard(m), ref_to_value(v) {}
    };
    struct ConstAccess {
        std::unique_lock<std::mutex> guard;
        const T &ref_to_value;
        ConstAccess(std::mutex &m, const T &v) : guard(m), ref_to_value(v) {}
    };
    Access GetAccess() { return {mutex_, value_}; }
    ConstAccess GetAccess() const { return {mutex_, value_}; }
};
template <class K, class V> class ConcurrentMap {
    static_assert(std::is_integral_v<K>, "ConcurrentMap supports only integer keys");
    struct Bucket {
        std::mutex mutex;
        std::map<K, V> values;
    };
    std::vector<Bucket> buckets_;

  public:
    struct Access {
        std::unique_lock<std::mutex> guard;
        V &ref_to_value;
        Access(Bucket &bucket, K key) : guard(bucket.mutex), ref_to_value(bucket.values[key]) {}
    };
    explicit ConcurrentMap(std::size_t count) : buckets_(count) {
        if (!count)
            throw std::invalid_argument("bucket count is zero");
    }
    Access operator[](K key) {
        return {buckets_[static_cast<std::uint64_t>(key) % buckets_.size()], key};
    }
    std::map<K, V> BuildOrdinaryMap() {
        std::map<K, V> result;
        for (auto &b : buckets_) {
            std::lock_guard<std::mutex> lock(b.mutex);
            result.insert(b.values.begin(), b.values.end());
        }
        return result;
    }
};
inline std::size_t WorkerCount() {
    return std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
}
inline std::int64_t CalculateMatrixSum(const std::vector<std::vector<int>> &matrix) {
    std::vector<std::future<std::int64_t>> jobs;
    const auto block =
        std::max<std::size_t>(1, (matrix.size() + WorkerCount() - 1) / WorkerCount());
    for (std::size_t begin = 0; begin < matrix.size(); begin += block)
        jobs.push_back(std::async(std::launch::async, [&, begin] {
            std::int64_t sum = 0;
            for (auto i = begin; i < std::min(begin + block, matrix.size()); ++i)
                for (int x : matrix[i])
                    sum += x;
            return sum;
        }));
    std::int64_t result = 0;
    for (auto &job : jobs)
        result += job.get();
    return result;
}
namespace keywords {
struct Stats {
    std::map<std::string, int> word_frequences;
    void operator+=(const Stats &other) {
        for (const auto &[word, count] : other.word_frequences)
            word_frequences[word] += count;
    }
};
inline Stats ExploreKeyWords(const std::set<std::string> &keys, std::istream &input) {
    std::vector<std::future<Stats>> jobs;
    Stats result;
    std::vector<std::string> batch;
    auto dispatch = [&] {
        jobs.push_back(std::async(std::launch::async, [&keys, lines = std::move(batch)] {
            Stats local;
            for (const auto &line : lines) {
                std::size_t begin = 0;
                while (begin < line.size()) {
                    begin = line.find_first_not_of(' ', begin);
                    if (begin == line.npos)
                        break;
                    auto end = line.find(' ', begin);
                    auto word = line.substr(begin, end - begin);
                    if (keys.count(word))
                        ++local.word_frequences[word];
                    if (end == line.npos)
                        break;
                    begin = end;
                }
            }
            return local;
        }));
        batch.clear();
        if (jobs.size() >= WorkerCount()) {
            result += jobs.front().get();
            jobs.erase(jobs.begin());
        }
    };
    for (std::string line; std::getline(input, line);) {
        batch.push_back(std::move(line));
        if (batch.size() == 2048)
            dispatch();
    }
    if (!batch.empty())
        dispatch();
    for (auto &job : jobs)
        result += job.get();
    if (input.bad())
        throw std::runtime_error("keyword input failed");
    return result;
}
} // namespace keywords
} // namespace museum::red
