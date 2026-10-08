"""Measure MPI scaling on one host; construction and correctness checks are untimed."""
import csv
import json
import os
import platform
import shlex
import statistics
import subprocess
import sys
from datetime import date
from pathlib import Path

from mpi_runner import launch

ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "projects/numerical-methods/mpi/results"
FLAGS = "-std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread"
compiler = os.environ.get("MPICXX", "mpicxx")
build_dir = Path(os.environ.get("MPI_BUILD", "build"))
subprocess.run(["make", "-B", str(build_dir / "benchmark-mpi"), f"MPI_BUILD={build_dir}", f"MPICXX={compiler}", f"CXXFLAGS={FLAGS}"],
               cwd=ROOT, check=True, timeout=60)
rows = []
for ranks in (1, 2, 4):
    result = launch(ranks, ROOT / build_dir / "benchmark-mpi", timeout=120)
    if result.returncode:
        raise RuntimeError(result.stderr or result.stdout)
    batch = json.loads(result.stdout)
    for run in batch["runs"]:
        rows.append(dict(ranks=ranks, **run,
                         median_elimination_seconds=statistics.median(run["elimination_seconds"]),
                         median_distributed_seconds=statistics.median(run["distributed_seconds"])))
    print(f"Measured {ranks} ranks", flush=True)
version = subprocess.run(shlex.split(os.environ.get("MPIEXEC", "mpiexec")) + ["--version"],
                         capture_output=True, text=True, timeout=10)
data = dict(recorded_on=date.today().isoformat(), seed=20261005, samples=3, warmups=1,
            environment=dict(system=platform.system(), architecture=platform.machine(),
                             logical_cpu_count=os.cpu_count(),
                             mpi_version=(version.stdout or version.stderr).splitlines()[:4],
                             compiler=subprocess.check_output([compiler, "--version"], text=True).splitlines()[0],
                             flags=FLAGS, launcher_flags=os.environ.get("MPIEXEC_FLAGS", ""),
                             placement="single host, no CPU affinity requested"),
            scope="Maximum per-rank elapsed duration, not differences between clocks on different ranks. "
                  "Distributed timing includes allocation, input packing/scatter, elimination, inverse rescaling, "
                  "gather and root reconstruction. Excludes MPI startup, input generation, file I/O and residual checks. "
                  "Elimination timing includes pivot communication, computation, error agreement and final rescaling.",
            runs=rows)
DESTINATION.mkdir(parents=True, exist_ok=True)
(DESTINATION / "benchmark.json").write_text(json.dumps(data, indent=2) + "\n")
with (DESTINATION / "benchmark.csv").open("w", newline="") as stream:
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(["n", "ranks", "median_elimination_seconds", "median_distributed_seconds"])
    writer.writerows((row["n"], row["ranks"], row["median_elimination_seconds"], row["median_distributed_seconds"]) for row in rows)

subprocess.run([sys.executable, str(ROOT / "scripts/plot_mpi.py")], check=True, timeout=30)
print(DESTINATION.relative_to(ROOT), flush=True)
