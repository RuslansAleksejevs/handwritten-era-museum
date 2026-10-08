#pragma once

#include "../include/matrix.hpp"
#include <mpi.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <stdexcept>

namespace preai::distributed {

namespace detail {

// An MPI communication failure is not a recoverable numerical/input error.
inline void Check(int error, const char* operation, MPI_Comm comm) {
    if (error == MPI_SUCCESS) return;
    char message[MPI_MAX_ERROR_STRING]{};
    int length = 0;
    MPI_Error_string(error, message, &length);
    std::fprintf(stderr, "%s failed: %.*s\n", operation, length, message);
    MPI_Abort(comm, error);
    std::abort();
}

// Every rank calls this in the same order. The callback contains no collectives.
// Publish the first failing rank's bounded diagnostic before throwing everywhere.
template <class Function>
void CollectiveStage(MPI_Comm comm, Function function) {
    int rank = 0, size = 0;
    Check(MPI_Comm_rank(comm, &rank), "MPI_Comm_rank", comm);
    Check(MPI_Comm_size(comm, &size), "MPI_Comm_size", comm);
    int failed_rank = size;
    std::array<char, 256> message{};
    try {
        function();
    } catch (const std::exception& error) {
        failed_rank = rank;
        std::strncpy(message.data(), error.what(), message.size() - 1);
    } catch (...) {
        failed_rank = rank;
        std::strncpy(message.data(), "unknown local failure", message.size() - 1);
    }
    int first = size;
    Check(MPI_Allreduce(&failed_rank, &first, 1, MPI_INT, MPI_MIN, comm), "MPI_Allreduce", comm);
    if (first != size) {
        Check(MPI_Bcast(message.data(), static_cast<int>(message.size()), MPI_CHAR, first, comm),
              "MPI_Bcast(error)", comm);
        throw std::runtime_error(message.data());
    }
}

}  // namespace detail

struct Result {
    std::optional<Matrix> inverse;  // Present only on rank 0 of the supplied communicator.
    double elimination_seconds = 0;  // Maximum local duration, meaningful on rank 0.
    double distributed_seconds = 0;  // Allocation/packing/scatter/solve/gather, excluding I/O.
};

// Collective, single-threaded per rank. Only rank 0 supplies a matrix; others pass nullptr.
// All ranks receive expected input/numerical failures and can reuse the communicator.
// Unexpected MPI failures abort comm. This is not a process-failure-tolerant solver.
Result Inverse(const Matrix* root_input, MPI_Comm comm = MPI_COMM_WORLD);

}  // namespace preai::distributed
