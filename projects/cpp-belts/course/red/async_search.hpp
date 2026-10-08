#pragma once
#include "../../search/search.hpp"
#include <exception>
#include <future>
#include <mutex>
#include <vector>
namespace museum::red {
// Part 2 owns asynchronous jobs. Input/output streams must outlive Wait() or the
// destructor, just as in the statement's test harness. Distinct jobs use distinct
// streams; construction loads the initial index synchronously.
class AsyncSearchServer {
    museum::search::SearchServer server_;
    std::mutex jobs_mutex_;
    std::vector<std::future<void>> jobs_;
    template <class Function> void Start(Function function) {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        jobs_.push_back(std::async(std::launch::async, std::move(function)));
    }

  public:
    AsyncSearchServer() = default;
    explicit AsyncSearchServer(std::istream &documents) : server_(documents) {}
    ~AsyncSearchServer() {
        try {
            Wait();
        } catch (...) { /* Destructors cannot report worker exceptions: call Wait explicitly. */
        }
    }
    void UpdateDocumentBase(std::istream &documents) {
        Start([this, &documents] { server_.UpdateDocumentBase(documents); });
    }
    void AddQueriesStream(std::istream &queries, std::ostream &output) {
        Start([this, &queries, &output] { server_.AddQueriesStream(queries, output); });
    }
    void Wait() {
        std::vector<std::future<void>> jobs;
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            jobs.swap(jobs_);
        }
        std::exception_ptr error;
        for (auto &job : jobs)
            try {
                job.get();
            } catch (...) {
                if (!error)
                    error = std::current_exception();
            }
        if (error)
            std::rethrow_exception(error);
    }
};
} // namespace museum::red
