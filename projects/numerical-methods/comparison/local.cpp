#include "common.hpp"
#include "../include/solver.hpp"
#include <chrono>

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument("usage: compare-local N WORKERS REPEATS");
        const int n = comparison::positive(argv[1], 4096);
        const int workers = comparison::positive(argv[2], 64);
        const int repeats = comparison::positive(argv[3], 20);
        const auto a = comparison::matrix(n);
        const auto hash = comparison::fingerprint(a);
        comparison::header();
        for (int trial = -1; trial < repeats; ++trial) {
            const auto start = std::chrono::steady_clock::now();
            auto result = preai::inverse(a, workers);
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            // The inverse API already computes A*E-I and its normalized residual.
            // Right residual is an extra verification, outside the timed region.
            const double right = preai::inverse_residual(result.inverse, a);
            if (trial >= 0)
                comparison::row(workers == 1 ? "serial" : "threads", workers, n, trial, hash,
                                seconds, result.residual, right, result.relative_residual);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
