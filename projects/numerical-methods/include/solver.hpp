#pragma once

#include "matrix.hpp"
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace preai {
// Fixed workers, coordinated by the caller. No worker decides to leave a pivot
// barrier: singularity is handled by the caller between completed operations.
class ColumnWorkers {
    std::mutex mutex_;
    std::condition_variable ready_, done_;
    std::vector<std::thread> workers_;
    std::function<void(std::size_t)> operation_;
    std::exception_ptr error_;
    std::size_t generation_=0, remaining_=0, columns_=0;
    bool stop_=false;

    void close() noexcept {
        { std::lock_guard<std::mutex> lock(mutex_); stop_=true; }
        ready_.notify_all();
        for (auto& w:workers_) if (w.joinable()) w.join();
    }
public:
    explicit ColumnWorkers(std::size_t count) {
        if (!count) throw std::invalid_argument("worker count must be positive");
        try {
            for (std::size_t id=0;id<count;++id) workers_.emplace_back([this,id,count]{
                std::size_t seen=0;
                std::unique_lock<std::mutex> lock(mutex_);
                for (;;) {
                    ready_.wait(lock,[&]{return stop_ || seen!=generation_;});
                    if (stop_) return;
                    seen=generation_;
                    lock.unlock();
                    std::exception_ptr failure;
                    try { for (std::size_t j=id;j<columns_;j+=count) operation_(j); }
                    catch (...) { failure=std::current_exception(); }
                    lock.lock();
                    if (failure && !error_) error_=failure;
                    if (--remaining_==0) done_.notify_one();
                }
            });
        } catch (...) { close(); throw; }
    }
    ColumnWorkers(const ColumnWorkers&)=delete;
    ColumnWorkers& operator=(const ColumnWorkers&)=delete;
    ~ColumnWorkers() { close(); }
    void run(std::size_t columns, std::function<void(std::size_t)> operation) {
        std::unique_lock<std::mutex> lock(mutex_);
        operation_=std::move(operation); columns_=columns; error_=nullptr;
        remaining_=workers_.size(); ++generation_;
        ready_.notify_all();
        done_.wait(lock,[&]{return remaining_==0;});
        if (error_) std::rethrow_exception(error_);
    }
};

struct InverseResult { Matrix inverse; double residual; double relative_residual; };

// Same column-operation idea as the historical solver: A E starts at A and
// remains equal to the transformed matrix. Finishing with A E = I gives E=A^-1.
inline InverseResult inverse(const Matrix& input, std::size_t threads=1) {
    if (input.rows()!=input.cols()) throw std::invalid_argument("matrix must be square");
    if (!threads || threads>256) throw std::invalid_argument("threads must be in [1,256]");
    const auto n=input.rows();
    double scale=0;
    for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<n;++j) {
        if (!std::isfinite(input(i,j))) throw std::invalid_argument("matrix must be finite");
        scale=std::max(scale,std::abs(input(i,j)));
    }
    if (scale==0) throw std::domain_error("singular matrix");
    // Scale first so uniformly very small/large systems are handled consistently.
    Matrix a=input, e=Matrix::identity(n);
    for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<n;++j) a(i,j)/=scale;
    const double tolerance=32*std::numeric_limits<double>::epsilon()*static_cast<double>(n);
    std::unique_ptr<ColumnWorkers> pool;
    if (threads>1) pool=std::make_unique<ColumnWorkers>(std::min(threads,n));
    for (std::size_t k=0;k<n;++k) {
        std::size_t pivot=k;
        for (std::size_t j=k+1;j<n;++j) if (std::abs(a(k,j))>std::abs(a(k,pivot))) pivot=j;
        if (std::abs(a(k,pivot))<=tolerance)
            throw std::domain_error("singular or numerically unresolved pivot");
        a.swap_columns(k,pivot); e.swap_columns(k,pivot);
        const double p=a(k,k);
        for (std::size_t i=0;i<n;++i) { a(i,k)/=p; e(i,k)/=p; }
        const auto eliminate=[&](std::size_t j) {
            if (j==k) return;
            const double f=a(k,j);
            for (std::size_t i=0;i<n;++i) { e(i,j)-=f*e(i,k); a(i,j)-=f*a(i,k); }
            a(k,j)=0;
        };
        if (pool) pool->run(n,eliminate);
        else for (std::size_t j=0;j<n;++j) eliminate(j);
    }
    for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<n;++j) {
        e(i,j)/=scale;
        if (!std::isfinite(e(i,j))) throw std::overflow_error("inverse exceeds double range");
    }
    const double residual=inverse_residual(input,e);
    const double relative=normalized_residual(residual,infinity_norm(input),infinity_norm(e));
    return {std::move(e),residual,relative};
}
} // namespace preai
