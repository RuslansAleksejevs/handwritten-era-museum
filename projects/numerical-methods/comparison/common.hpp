#pragma once
#include "../include/matrix.hpp"
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

namespace comparison {
inline int positive(const char* argument, int maximum) {
    const std::string text(argument);
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("positive integer required");
    const auto value = std::stoul(text);
    if (value == 0 || value > static_cast<unsigned>(maximum))
        throw std::invalid_argument("benchmark argument outside supported range");
    return static_cast<int>(value);
}

inline preai::Matrix matrix(int n) {
    preai::Matrix out(n, n);
    std::mt19937 generator(20261007);
    for (int i = 0; i < n; ++i) {
        double sum = 0;
        for (int j = 0; j < n; ++j) {
            const double value = double(generator()) / double(std::mt19937::max()) * 2 - 1;
            out(i, j) = value;
            sum += std::abs(value);
        }
        out(i, i) += sum + 1;
    }
    return out;
}

inline std::string fingerprint(const preai::Matrix& input) {
    // FNV-1a of row-major IEEE double bits, byte order specified explicitly.
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < input.rows(); ++i)
        for (std::size_t j = 0; j < input.cols(); ++j) {
            std::uint64_t bits;
            const double value = input(i, j);
            std::memcpy(&bits, &value, sizeof(bits));
            for (int byte = 0; byte < 8; ++byte) {
                hash ^= (bits >> (byte * 8)) & 255;
                hash *= 1099511628211ULL;
            }
        }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

inline void header() {
    std::cout << "backend,workers,n,trial,matrix_fingerprint,checked_seconds,left_residual_inf,right_residual_inf,relative_residual\n"
              << std::setprecision(17);
}

inline void row(const char* backend, int workers, int n, int trial, const std::string& hash,
                double seconds, double left, double right, double relative) {
    if (!(seconds > 0) || !std::isfinite(seconds) || !std::isfinite(left) || !std::isfinite(right) ||
        !std::isfinite(relative) || left < 0 || right < 0 || relative < 0 || std::max(left, right) > 1e-10)
        throw std::runtime_error("benchmark result failed timing/residual validation");
    std::cout << backend << ',' << workers << ',' << n << ',' << trial << ',' << hash << ','
              << seconds << ',' << left << ',' << right << ',' << relative << '\n';
}
} // namespace comparison
