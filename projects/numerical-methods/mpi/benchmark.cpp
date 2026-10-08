#include "solver.hpp"

#include <array>
#include <iomanip>
#include <iostream>
#include <random>

int main(int argc, char** argv) {
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) return 2;
    const auto comm = MPI_COMM_WORLD;
    int rank = 0, size = 0;
    MPI_Comm_rank(comm, &rank); MPI_Comm_size(comm, &size);
    using preai::distributed::detail::CollectiveStage;
    try {
        if (rank == 0) std::cout << std::setprecision(17) << "{\"ranks\":" << size << ",\"runs\":[";
        bool first = true;
        for (const int n : {64, 128, 256, 512}) {
            std::optional<preai::Matrix> matrix, answer;
            CollectiveStage(comm, [&] {
                if (rank != 0) return;
                matrix.emplace(n, n);
                std::mt19937 random(20261005);
                for (int row = 0; row < n; ++row) {
                    double sum = 0;
                    for (int col = 0; col < n; ++col) {
                        const double value = (static_cast<double>(random()) / std::mt19937::max()) * 2 - 1;
                        (*matrix)(row,col) = value; sum += std::abs(value);
                    }
                    (*matrix)(row,row) += sum + 1;
                }
            });
            std::array<double,3> kernel{}, total{};
            for (int repeat = -1; repeat < 3; ++repeat) {
                auto result = preai::distributed::Inverse(rank == 0 ? &*matrix : nullptr, comm);
                if (rank == 0 && repeat >= 0) {
                    kernel[repeat] = result.elimination_seconds;
                    total[repeat] = result.distributed_seconds;
                    if (repeat == 2) answer = std::move(result.inverse);
                }
            }
            CollectiveStage(comm, [&] {
                if (rank != 0) return;
                const double left = preai::inverse_residual(*matrix, *answer);
                const double right = preai::inverse_residual(*answer, *matrix);
                if (std::max(left,right) > 1e-10) throw std::runtime_error("benchmark inverse failed residual check");
                if (!first) std::cout << ',';
                first = false;
                std::cout << "{\"n\":" << n << ",\"elimination_seconds\":[";
                for (int i=0;i<3;++i) std::cout << (i ? "," : "") << kernel[i];
                std::cout << "],\"distributed_seconds\":[";
                for (int i=0;i<3;++i) std::cout << (i ? "," : "") << total[i];
                std::cout << "],\"left_residual_inf\":" << left << ",\"right_residual_inf\":" << right << '}';
            });
        }
        if (rank == 0) std::cout << "]}\n";
    } catch (const std::exception& error) {
        if (rank == 0) std::cerr << error.what() << '\n';
        MPI_Abort(comm, 1); return 1;
    }
    MPI_Finalize();
}
