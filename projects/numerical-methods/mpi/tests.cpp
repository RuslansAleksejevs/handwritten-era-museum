#include "solver.hpp"
#include "../include/solver.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

namespace {
using preai::Matrix;
using preai::distributed::Inverse;
using preai::distributed::detail::Check;
using preai::distributed::detail::CollectiveStage;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <class Function>
void ExpectFailure(MPI_Comm comm, Function function, const char* fragment) {
    bool caught = false;
    try { function(); }
    catch (const std::exception& error) { caught = std::string(error.what()).find(fragment) != std::string::npos; }
    CollectiveStage(comm, [&] { Require(caught, "not every rank received the expected failure"); });
}

Matrix Seeded(int n, unsigned seed, double scale = 1) {
    Matrix matrix(n, n);
    std::mt19937 random(seed);
    std::uniform_real_distribution<double> value(-1, 1);
    for (int row = 0; row < n; ++row) {
        double sum = 0;
        for (int col = 0; col < n; ++col) {
            matrix(row, col) = value(random);
            sum += std::abs(matrix(row, col));
        }
        matrix(row, row) += sum + 1;
    }
    for (int row = 0; row < n; ++row) for (int col = 0; col < n; ++col) matrix(row, col) *= scale;
    return matrix;
}

void Verify(const Matrix* input, MPI_Comm comm) {
    int rank = 0;
    Check(MPI_Comm_rank(comm, &rank), "MPI_Comm_rank", comm);
    const auto result = Inverse(input, comm);
    CollectiveStage(comm, [&] {
        Require(result.inverse.has_value() == (rank == 0), "inverse belongs only to communicator root");
        if (rank != 0) return;
        const auto& answer = *result.inverse;
        const double left = preai::inverse_residual(*input, answer);
        const double right = preai::inverse_residual(answer, *input);
        Require(left < 1e-11 && right < 1e-11, "inverse residual too large");
        const auto reference = preai::inverse(*input);
        double error = 0, magnitude = 0;
        for (std::size_t i = 0; i < answer.rows(); ++i) for (std::size_t j = 0; j < answer.cols(); ++j) {
            error = std::max(error, std::abs(answer(i,j) - reference.inverse(i,j)));
            magnitude = std::max(magnitude, std::abs(reference.inverse(i,j)));
        }
        Require(error <= 1e-12 * magnitude, "MPI result differs from serial result");
        Require(result.elimination_seconds >= 0 && result.distributed_seconds >= result.elimination_seconds,
                "invalid elapsed duration");
    });
}

int Suite(MPI_Comm comm) {
    int rank = 0, size = 0, cases = 0;
    Check(MPI_Comm_rank(comm, &rank), "MPI_Comm_rank", comm);
    Check(MPI_Comm_size(comm, &size), "MPI_Comm_size", comm);
    for (const int n : {1, 2, 3, 5, 7, 12, 17, 31}) {
        for (unsigned seed = 0; seed < 4; ++seed) {
            for (const double scale : {1.0, 1e-150, 1e150}) {
                std::unique_ptr<Matrix> input;
                CollectiveStage(comm, [&] { if (rank == 0) input = std::make_unique<Matrix>(Seeded(n, seed, scale)); });
                Verify(input.get(), comm);
                ++cases;
            }
        }
    }
    // A reversed permutation demands column swaps, including across owners.
    for (const int n : {2, 3, 7, 8}) {
        std::unique_ptr<Matrix> permutation;
        CollectiveStage(comm, [&] {
            if (rank != 0) return;
            permutation = std::make_unique<Matrix>(n, n);
            for (int row = 0; row < n; ++row) (*permutation)(row, n - row - 1) = row + 1;
        });
        Verify(permutation.get(), comm);
        ++cases;
    }
    // Equal-magnitude candidates exercise deterministic MAXLOC tie breaking.
    std::unique_ptr<Matrix> tied;
    CollectiveStage(comm, [&] {
        if (rank != 0) return;
        tied = std::make_unique<Matrix>(2, 2);
        (*tied)(0,0)=1; (*tied)(0,1)=-1; (*tied)(1,0)=1; (*tied)(1,1)=1;
    });
    Verify(tied.get(), comm); ++cases;

    ExpectFailure(comm, [&] { Inverse(nullptr, comm); }, "root must supply");
    for (int kind = 0; kind < 6; ++kind) {
        std::unique_ptr<Matrix> bad;
        CollectiveStage(comm, [&] {
            if (rank != 0) return;
            bad = std::make_unique<Matrix>(2, kind == 0 ? 3 : 2);
            if (kind == 1) (*bad)(0,0) = std::numeric_limits<double>::quiet_NaN();
            if (kind == 2) (*bad)(0,0) = std::numeric_limits<double>::infinity();
            if (kind == 3) { (*bad)(0,0)=1; (*bad)(0,1)=2; (*bad)(1,0)=2; (*bad)(1,1)=4; }
            if (kind == 5) { (*bad)(0,0)=1; (*bad)(1,1)=1e-18; }
        });
        const char* message = kind == 0 ? "root must supply" : kind < 3 ? "finite" : "singular";
        ExpectFailure(comm, [&] { Inverse(bad.get(), comm); }, message);
    }
    // Only the owning rank has a value that overflows at final rescaling.
    std::unique_ptr<Matrix> tiny;
    CollectiveStage(comm, [&] {
        if (rank == 0) { tiny = std::make_unique<Matrix>(1,1); (*tiny)(0,0)=1e-320; }
    });
    ExpectFailure(comm, [&] { Inverse(tiny.get(), comm); }, "inverse exceeds");

    // An error originating at a non-root rank must reach every participant.
    ExpectFailure(comm, [&] {
        CollectiveStage(comm, [&] { if (rank == size - 1) throw std::runtime_error("injected worker failure"); });
    }, "injected worker failure");
    ExpectFailure(comm, [&] {
        CollectiveStage(comm, [&] { if (rank == size - 1) throw std::bad_alloc(); });
    }, "bad_alloc");
    // The communicator is reusable after all expected failures.
    Verify(tied.get(), comm); ++cases;
    return cases;
}
}  // namespace

int main(int argc, char** argv) {
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) return 2;
    int rank = 0, size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    try {
        const auto cases = Suite(MPI_COMM_WORLD);
        MPI_Comm split;
        Check(MPI_Comm_split(MPI_COMM_WORLD, rank % 2, size - rank, &split), "MPI_Comm_split", MPI_COMM_WORLD);
        int local_rank = 0;
        MPI_Comm_rank(split, &local_rank);
        std::unique_ptr<Matrix> input;
        CollectiveStage(split, [&] { if (local_rank == 0) input = std::make_unique<Matrix>(Seeded(5, rank % 2)); });
        Verify(input.get(), split);
        Check(MPI_Comm_free(&split), "MPI_Comm_free", MPI_COMM_WORLD);
        if (rank == 0) std::cout << "PASS: " << cases << " inversions at " << size
            << " ranks; collective failures, empty workers, split communicators\n";
    } catch (const std::exception& error) {
        std::cerr << "rank " << rank << ": " << error.what() << '\n';
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
    MPI_Finalize();
}
