"""Distributed correctness, failure termination and NumPy/LAPACK oracle checks."""
import argparse
import json
import os
import platform
import shlex
import subprocess
import tempfile
from datetime import date
from pathlib import Path

from mpi_runner import launch

ROOT = Path(__file__).resolve().parents[1]
BINARY_DIR = ROOT / os.environ.get("MPI_BUILD", "build")
RANKS = (1, 2, 3, 4, 6)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-binary", default=str(BINARY_DIR / "test-mpi"))
    parser.add_argument("--cpp-only", action="store_true")
    parser.add_argument("--record", type=Path)
    args = parser.parse_args()
    for ranks in RANKS:
        result = launch(ranks, ROOT / args.test_binary)
        require(result.returncode == 0, result.stdout + result.stderr)
        print(result.stdout.strip(), flush=True)
    if args.cpp_only:
        return

    import numpy as np

    rng = np.random.default_rng(20261005)
    matrices = []
    for n in (1, 2, 3, 7, 13):
        for _ in range(2):
            matrix = rng.uniform(-1, 1, (n, n))
            matrix += np.diag(np.sum(np.abs(matrix), axis=1) + 1)
            matrices.append((matrix, "diagonally_dominant"))
    matrices += [(np.fliplr(np.diag(np.arange(1., 8.))), "column_permutation"),
                 (np.array([[1., -1.], [1., 1.]]), "equal_pivots")]
    for scale in (1e-150, 1e150):
        matrices.append((matrices[7][0] * scale, f"uniform_scale_{scale}"))
    matrices.append((np.array([[1e-308]]), "representable_subnormal"))
    comparisons = []
    failures_checked = 0
    executable = BINARY_DIR / "inverse-mpi"
    with tempfile.TemporaryDirectory(prefix="museum-mpi-check-") as folder:
        path = Path(folder) / "matrix.txt"
        for matrix, kind in matrices:
            expected = np.linalg.inv(matrix)
            n = len(matrix)
            np.savetxt(path, matrix, header=str(n), comments="", fmt="%.17g")
            for ranks in RANKS:
                result = launch(ranks, executable, [path], timeout=20)
                require(result.returncode == 0, result.stderr)
                answer = np.fromstring(result.stdout, sep=" ")
                require(answer.size == n*n, "root must print exactly one inverse matrix")
                answer = answer.reshape(n, n)
                error = float(np.max(np.abs(answer - expected)) / np.max(np.abs(expected)))
                left = float(np.linalg.norm(matrix @ answer - np.eye(n), ord=np.inf))
                right = float(np.linalg.norm(answer @ matrix - np.eye(n), ord=np.inf))
                require(np.isfinite(answer).all() and error < 5e-12 and max(left, right) < 1e-10,
                        f"NumPy disagreement: {kind}, n={n}, ranks={ranks}")
                diagnostic = json.loads(result.stderr)
                require(diagnostic["ranks"] == ranks and diagnostic["dimension"] == n, "bad diagnostic")
                require(diagnostic["distributed_seconds"] >= diagnostic["elimination_seconds"] >= 0,
                        "invalid timing scope")
                comparisons.append(dict(kind=kind, n=n, ranks=ranks, relative_entrywise_error=error,
                                        left_residual_inf=left, right_residual_inf=right))
        # A well-conditioned matrix whose scale made the old diagnostic underflow.
        path.write_text("2\n5e307 2.5e307\n2.5e307 5e307\n")
        for ranks in RANKS:
            result = launch(ranks, executable, [path], timeout=20)
            require(result.returncode == 0, result.stderr)
            diagnostic = json.loads(result.stderr)
            residual = max(diagnostic["left_residual_inf"], diagnostic["right_residual_inf"])
            relative = diagnostic["relative_residual_inf"]
            require(residual > 0 and relative > 0 and abs(relative/(residual/3)-1) < 1e-13,
                    "large-scale relative residual lost precision")
        bad_inputs = [
            ("", "n must be"), ("0\n", "n must be"), ("-2\n", "n must be"),
            ("4097\n", "n must be"), ("2.5 1 0 1\n", "n must be"), ("2\n1 2 3\n", "matrix entry"),
            ("1\n1 trailing\n", "trailing"), ("1\nnan\n", "matrix"),
            ("1\ninf\n", "matrix"), ("1\n1e999\n", "matrix"),
            ("2\n1 2\n2 4\n", "singular"), ("1\n0\n", "singular"),
            ("1\n1e-320\n", "inverse exceeds"),
            ("1\n1e-999\n", "underflows"),
        ]
        for ranks in (1, 3, 6):
            for data, expected_message in bad_inputs:
                path.write_text(data)
                result = launch(ranks, executable, [path], timeout=15)
                require(result.returncode != 0 and expected_message in result.stderr, result.stderr)
                require(not result.stdout.strip(), "failed inversion must not print a matrix")
                failures_checked += 1
            for arguments, expected_message in (([], "usage"), ([Path(folder)/"missing"], "cannot open")):
                result = launch(ranks, executable, arguments, timeout=15)
                require(result.returncode != 0 and expected_message in result.stderr, result.stderr)
                failures_checked += 1
    print(f"PASS: {len(comparisons)} NumPy/LAPACK comparisons; {len(RANKS)} extreme-scale diagnostics; "
          f"{failures_checked} failing CLI runs terminated", flush=True)
    if args.record:
        launcher = shlex.split(os.environ.get("MPIEXEC", "mpiexec"))
        version = subprocess.run(launcher + ["--version"], capture_output=True, text=True, timeout=10)
        data = dict(recorded_on=date.today().isoformat(), seed=20261005,
                    environment=dict(system=platform.system(), architecture=platform.machine(),
                                     numpy=np.__version__, launcher_version=(version.stdout or version.stderr).splitlines()[:4]),
                    ranks=list(RANKS), cpp_inversions_per_rank_configuration=102,
                    cpp_split_communicators=True, cpp_collective_failure_cases_per_configuration=10,
                    cli_failure_runs=failures_checked, oracle_comparisons=len(comparisons),
                    max_relative_entrywise_error=max(x["relative_entrywise_error"] for x in comparisons),
                    max_left_residual_inf=max(x["left_residual_inf"] for x in comparisons),
                    max_right_residual_inf=max(x["right_residual_inf"] for x in comparisons),
                    comparisons=comparisons,
                    scope="Single-host MPI processes; NumPy independently checks both inverse products. No multi-host or process-crash resilience claim.")
        args.record.parent.mkdir(parents=True, exist_ok=True)
        args.record.write_text(json.dumps(data, indent=2) + "\n")


if __name__ == "__main__":
    main()
