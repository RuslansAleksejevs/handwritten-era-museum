#include "solver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace preai::distributed {
namespace {

using detail::Check;
using detail::CollectiveStage;
inline constexpr int kMaximumDimension = 4096;
inline constexpr int kSwapTag = 0;

int ColumnCount(int n, int rank, int size) noexcept {
    return rank < n ? 1 + (n - 1 - rank) / size : 0;
}

struct Columns {
    int rows = 0;
    int count = 0;
    std::vector<double> a;
    std::vector<double> e;

    double* A(int local) { return a.data() + static_cast<std::size_t>(local) * rows; }
    double* E(int local) { return e.data() + static_cast<std::size_t>(local) * rows; }
};

void Swap(Columns& columns, int left, int right, int rank, int size, MPI_Comm comm) {
    if (left == right) return;
    const int owner_left = left % size, owner_right = right % size;
    if (owner_left == owner_right) {
        if (rank == owner_left) {
            std::swap_ranges(columns.A(left / size), columns.A(left / size) + columns.rows,
                             columns.A(right / size));
            std::swap_ranges(columns.E(left / size), columns.E(left / size) + columns.rows,
                             columns.E(right / size));
        }
    } else if (rank == owner_left || rank == owner_right) {
        const int local = (rank == owner_left ? left : right) / size;
        const int peer = rank == owner_left ? owner_right : owner_left;
        Check(MPI_Sendrecv_replace(columns.A(local), columns.rows, MPI_DOUBLE, peer, kSwapTag,
                                   peer, kSwapTag, comm, MPI_STATUS_IGNORE), "MPI_Sendrecv_replace(A)", comm);
        Check(MPI_Sendrecv_replace(columns.E(local), columns.rows, MPI_DOUBLE, peer, kSwapTag,
                                   peer, kSwapTag, comm, MPI_STATUS_IGNORE), "MPI_Sendrecv_replace(E)", comm);
    }
}

}  // namespace

Result Inverse(const Matrix* root_input, MPI_Comm comm) {
    int rank = 0, size = 0, n = 0;
    Check(MPI_Comm_rank(comm, &rank), "MPI_Comm_rank", comm);
    Check(MPI_Comm_size(comm, &size), "MPI_Comm_size", comm);
    double scale = 0;
    CollectiveStage(comm, [&] {
        if (rank != 0) return;
        if (!root_input || root_input->rows() != root_input->cols() ||
            root_input->rows() > kMaximumDimension) {
            throw std::invalid_argument("root must supply a square matrix with n in [1,4096]");
        }
        n = static_cast<int>(root_input->rows());
        for (int row = 0; row < n; ++row) for (int col = 0; col < n; ++col) {
            const double value = (*root_input)(row, col);
            if (!std::isfinite(value)) throw std::invalid_argument("matrix must be finite");
            scale = std::max(scale, std::abs(value));
        }
        if (scale == 0) throw std::domain_error("singular matrix");
    });
    Check(MPI_Bcast(&n, 1, MPI_INT, 0, comm), "MPI_Bcast(n)", comm);
    Check(MPI_Bcast(&scale, 1, MPI_DOUBLE, 0, comm), "MPI_Bcast(scale)", comm);
    Check(MPI_Barrier(comm), "MPI_Barrier(timing)", comm);
    const double start = MPI_Wtime();

    Columns columns;
    std::vector<int> counts, offsets;
    std::vector<double> packed, pivot_a, pivot_e;
    CollectiveStage(comm, [&] {
        counts.resize(size); offsets.resize(size);
        int offset = 0;
        for (int worker = 0; worker < size; ++worker) {
            // n <= 4096 keeps counts and displacements within MPI's int range.
            counts[worker] = n * ColumnCount(n, worker, size);
            offsets[worker] = offset;
            offset += counts[worker];
        }
        columns.rows = n;
        columns.count = ColumnCount(n, rank, size);
        columns.a.resize(counts[rank]);
        columns.e.resize(counts[rank]);
        for (int local = 0; local < columns.count; ++local) {
            columns.E(local)[rank + local * size] = 1;
        }
        pivot_a.resize(n); pivot_e.resize(n);
        if (rank == 0) {
            packed.resize(static_cast<std::size_t>(n) * n);
            for (int worker = 0; worker < size; ++worker) {
                int position = offsets[worker];
                for (int col = worker; col < n; col += size) {
                    for (int row = 0; row < n; ++row) packed[position++] = (*root_input)(row, col) / scale;
                }
            }
        }
    });
    Check(MPI_Scatterv(packed.data(), counts.data(), offsets.data(), MPI_DOUBLE,
                       columns.a.data(), counts[rank], MPI_DOUBLE, 0, comm), "MPI_Scatterv", comm);
    // Release root's temporary input pack before allocating the output pack later.
    std::vector<double>().swap(packed);
    Check(MPI_Barrier(comm), "MPI_Barrier(kernel timing)", comm);
    const double elimination_start = MPI_Wtime();
    const double tolerance = 32 * std::numeric_limits<double>::epsilon() * n;

    for (int k = 0; k < n; ++k) {
        struct { double value; int column; } local_best{0, std::numeric_limits<int>::max()}, best{};
        for (int local = 0; local < columns.count; ++local) {
            const int col = rank + local * size;
            if (col >= k) {
                const double candidate = std::abs(columns.A(local)[k]);
                if (candidate > local_best.value ||
                    (candidate == local_best.value && col < local_best.column)) {
                    local_best = {candidate, col};
                }
            }
        }
        Check(MPI_Allreduce(&local_best, &best, 1, MPI_DOUBLE_INT, MPI_MAXLOC, comm),
              "MPI_Allreduce(pivot)", comm);
        if (best.value <= tolerance) throw std::domain_error("singular or numerically unresolved pivot");
        Swap(columns, k, best.column, rank, size, comm);
        const int owner = k % size;
        if (rank == owner) {
            auto* a = columns.A(k / size);
            auto* e = columns.E(k / size);
            const double pivot = a[k];
            for (int row = 0; row < n; ++row) { a[row] /= pivot; e[row] /= pivot; }
            a[k] = 1;
            std::copy(a, a + n, pivot_a.begin());
            std::copy(e, e + n, pivot_e.begin());
        }
        Check(MPI_Bcast(pivot_a.data(), n, MPI_DOUBLE, owner, comm), "MPI_Bcast(pivot A)", comm);
        Check(MPI_Bcast(pivot_e.data(), n, MPI_DOUBLE, owner, comm), "MPI_Bcast(pivot E)", comm);
        CollectiveStage(comm, [&] {
            for (int local = 0; local < columns.count; ++local) {
                auto* a = columns.A(local);
                auto* e = columns.E(local);
                if (rank + local * size != k) {
                    const double factor = a[k];
                    for (int row = 0; row < n; ++row) {
                        a[row] -= factor * pivot_a[row];
                        e[row] -= factor * pivot_e[row];
                    }
                    a[k] = 0;
                }
                for (int row = 0; row < n; ++row) {
                    if (!std::isfinite(a[row]) || !std::isfinite(e[row])) {
                        throw std::overflow_error("non-finite elimination result");
                    }
                }
            }
        });
    }
    CollectiveStage(comm, [&] {
        for (auto& value : columns.e) {
            value /= scale;
            if (!std::isfinite(value)) throw std::overflow_error("inverse exceeds double range");
        }
    });
    const double elimination_elapsed = MPI_Wtime() - elimination_start;

    Result result;
    CollectiveStage(comm, [&] {
        if (rank == 0) {
            packed.resize(static_cast<std::size_t>(n) * n);
            result.inverse.emplace(n, n);
        }
    });
    Check(MPI_Gatherv(columns.e.data(), counts[rank], MPI_DOUBLE, packed.data(), counts.data(),
                      offsets.data(), MPI_DOUBLE, 0, comm), "MPI_Gatherv", comm);
    CollectiveStage(comm, [&] {
        if (rank != 0) return;
        for (int worker = 0; worker < size; ++worker) {
            int position = offsets[worker];
            for (int col = worker; col < n; col += size) {
                for (int row = 0; row < n; ++row) (*result.inverse)(row, col) = packed[position++];
            }
        }
    });
    const double elapsed = MPI_Wtime() - start;
    Check(MPI_Reduce(&elimination_elapsed, &result.elimination_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, comm),
          "MPI_Reduce(kernel time)", comm);
    Check(MPI_Reduce(&elapsed, &result.distributed_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, comm),
          "MPI_Reduce(total time)", comm);
    return result;
}

}  // namespace preai::distributed
