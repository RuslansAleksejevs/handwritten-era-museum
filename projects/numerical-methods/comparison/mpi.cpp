#include "common.hpp"
#include "../mpi/solver.hpp"

int main(int argc, char** argv) {
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) return 2;
    const auto comm = MPI_COMM_WORLD;
    using preai::distributed::detail::Check;
    using preai::distributed::detail::CollectiveStage;
    int rank = 0, workers = 0;
    Check(MPI_Comm_rank(comm, &rank), "MPI_Comm_rank", comm);
    Check(MPI_Comm_size(comm, &workers), "MPI_Comm_size", comm);
    try {
        if (argc != 3) throw std::invalid_argument("usage: compare-mpi N REPEATS");
        const int n = comparison::positive(argv[1], 4096);
        const int repeats = comparison::positive(argv[2], 20);
        std::optional<preai::Matrix> a;
        std::string hash;
        CollectiveStage(comm, [&] {
            if (rank == 0) { a = comparison::matrix(n); hash = comparison::fingerprint(*a); }
        });
        if (rank == 0) comparison::header();
        for (int trial = -1; trial < repeats; ++trial) {
            Check(MPI_Barrier(comm), "MPI_Barrier", comm);
            const double start = MPI_Wtime();
            auto result = preai::distributed::Inverse(rank == 0 ? &*a : nullptr, comm);
            double left = 0, relative = 0;
            // Same diagnostic work as the local inverse API, included in time.
            CollectiveStage(comm, [&] {
                if (rank != 0) return;
                left = preai::inverse_residual(*a, *result.inverse);
                relative = preai::normalized_residual(left, preai::infinity_norm(*a),
                                                       preai::infinity_norm(*result.inverse));
            });
            const double elapsed = MPI_Wtime() - start;
            double seconds = 0;
            Check(MPI_Reduce(&elapsed, &seconds, 1, MPI_DOUBLE, MPI_MAX, 0, comm), "MPI_Reduce", comm);
            CollectiveStage(comm, [&] {
                if (rank != 0) return;
                const double right = preai::inverse_residual(*result.inverse, *a);
                if (trial >= 0)
                    comparison::row("mpi", workers, n, trial, hash, seconds, left, right, relative);
            });
        }
    } catch (const std::exception& error) {
        if (rank == 0) std::cerr << error.what() << '\n';
        MPI_Abort(comm, 1);
        return 1;
    }
    MPI_Finalize();
}
