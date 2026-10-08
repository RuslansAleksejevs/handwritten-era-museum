#include "solver.hpp"
#include "../include/matrix_input.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>

namespace {
// Construct only after MPI_Init succeeds; all ordinary exits finalize MPI.
struct SessionEnd {
    ~SessionEnd() { MPI_Finalize(); }
};
}  // namespace

int main(int argc, char** argv) {
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) return 2;
    const SessionEnd session;
    const MPI_Comm comm = MPI_COMM_WORLD;
    using preai::distributed::detail::Check;
    using preai::distributed::detail::CollectiveStage;
    Check(MPI_Comm_set_errhandler(comm, MPI_ERRORS_RETURN), "MPI_Comm_set_errhandler", comm);
    int rank = 0, size = 0;
    Check(MPI_Comm_rank(comm, &rank), "MPI_Comm_rank", comm);
    Check(MPI_Comm_size(comm, &size), "MPI_Comm_size", comm);
    try {
        std::unique_ptr<preai::Matrix> input;
        CollectiveStage(comm, [&] {
            if (rank != 0) return;
            if (argc != 2) throw std::invalid_argument("usage: inverse-mpi MATRIX.txt");
            std::ifstream stream(argv[1]);
            if (!stream) throw std::invalid_argument("cannot open matrix file");
            input = std::make_unique<preai::Matrix>(preai::read_matrix(stream));
        });
        auto result = preai::distributed::Inverse(input.get(), comm);
        CollectiveStage(comm, [&] {
            if (rank != 0) return;
            const auto& inverse = *result.inverse;
            const auto left = preai::inverse_residual(*input, inverse);
            const auto right = preai::inverse_residual(inverse, *input);
            const auto relative = preai::normalized_residual(
                std::max(left, right), preai::infinity_norm(*input), preai::infinity_norm(inverse));
            std::cout << std::setprecision(17);
            for (std::size_t row = 0; row < inverse.rows(); ++row) {
                for (std::size_t col = 0; col < inverse.cols(); ++col) {
                    std::cout << (col ? " " : "") << inverse(row, col);
                }
                std::cout << '\n';
            }
            std::cout.flush();
            if (!std::cout) throw std::runtime_error("matrix output write failed");
            std::cerr << std::setprecision(17)
                      << "{\"ranks\":" << size << ",\"dimension\":" << inverse.rows()
                      << ",\"left_residual_inf\":" << left << ",\"right_residual_inf\":" << right
                      << ",\"relative_residual_inf\":" << relative
                      << ",\"elimination_seconds\":" << result.elimination_seconds
                      << ",\"distributed_seconds\":" << result.distributed_seconds << "}\n";
        });
    } catch (const std::exception& error) {
        if (rank == 0) std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
