"""Comparable checked-inverse calls on byte-identical inputs, on one host."""
import argparse
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
from mpi_runner import launch

ROOT = Path(__file__).resolve().parents[1]


def source_fingerprints():
    paths = ['scripts/compare_parallel.py', 'scripts/mpi_runner.py',
             'projects/numerical-methods/comparison/common.hpp',
             'projects/numerical-methods/comparison/local.cpp',
             'projects/numerical-methods/comparison/mpi.cpp',
             'projects/numerical-methods/include/matrix.hpp',
             'projects/numerical-methods/include/solver.hpp',
             'projects/numerical-methods/mpi/solver.hpp',
             'projects/numerical-methods/mpi/solver.cpp']
    return {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in paths}


def checked_rows(text, backend, workers, n, repeats):
    rows = list(csv.DictReader(io.StringIO(text)))
    if len(rows) != repeats or [int(r['trial']) for r in rows] != list(range(repeats)):
        raise ValueError('Missing or repeated measurement')
    for row in rows:
        if (row['backend'], int(row['workers']), int(row['n'])) != (backend, workers, n):
            raise ValueError('Benchmark identity mismatch')
        for field in ('checked_seconds', 'left_residual_inf', 'right_residual_inf', 'relative_residual'):
            value = float(row[field])
            if not math.isfinite(value) or value < 0:
                raise ValueError('Invalid benchmark value')
        if float(row['checked_seconds']) <= 0 or max(float(row[k]) for k in ('left_residual_inf', 'right_residual_inf')) > 1e-10:
            raise ValueError('Failed time or residual check')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'projects/numerical-methods/comparison/results')
    parser.add_argument('--sizes', type=int, nargs='+', default=[32, 128, 256, 512])
    parser.add_argument('--repeats', type=int, default=5)
    args = parser.parse_args()
    if not 1 <= args.repeats <= 20 or not args.sizes or min(args.sizes) < 1 or max(args.sizes) > 4096 or len(set(args.sizes)) != len(args.sizes):
        parser.error('Distinct sizes 1..4096 and repeats 1..20 required')
    if (args.output / 'measurements.csv').exists():
        parser.error('Results already exist; choose a fresh --output directory')
    mpi_build = Path(os.environ.get('MPI_BUILD', str(ROOT / 'build'))).resolve()
    subprocess.run(['make', f'MPI_BUILD={mpi_build}', 'build/compare-local', str(mpi_build / 'compare-mpi')], cwd=ROOT, check=True)
    configurations = [('serial', 1), ('threads', 2), ('threads', 4), ('mpi', 1), ('mpi', 2), ('mpi', 4)]
    cases = [(n, backend, workers) for n in args.sizes for backend, workers in configurations]
    random.Random(20261007).shuffle(cases)
    rows, commands = [], []
    fingerprints = {}
    for n, backend, workers in cases:
        if backend == 'mpi':
            result = launch(workers, mpi_build / 'compare-mpi', [n, args.repeats], timeout=180)
        else:
            result = subprocess.run([str(ROOT / 'build/compare-local'), str(n), str(workers), str(args.repeats)],
                                    text=True, capture_output=True, timeout=180)
        if result.returncode:
            raise RuntimeError(result.stderr)
        batch = checked_rows(result.stdout, backend, workers, n, args.repeats)
        for row in batch:
            prior = fingerprints.setdefault(n, row['matrix_fingerprint'])
            if row['matrix_fingerprint'] != prior:
                raise ValueError('Backends did not use identical matrix bytes')
        rows.extend(batch)
        commands.append({'n': n, 'backend': backend, 'workers': workers})
        print(f'{backend} {workers}, n={n}: {statistics.median(float(r["checked_seconds"]) for r in batch):.6f}s', flush=True)
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / 'measurements.csv').open('w', newline='') as dest:
        writer = csv.DictWriter(dest, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    summaries = []
    for n in args.sizes:
        baseline = statistics.median(float(r['checked_seconds']) for r in rows if int(r['n']) == n and r['backend'] == 'serial')
        for backend, workers in configurations:
            seconds = [float(r['checked_seconds']) for r in rows if int(r['n']) == n and r['backend'] == backend and int(r['workers']) == workers]
            median = statistics.median(seconds)
            summaries.append({'n': n, 'backend': backend, 'workers': workers, 'median_seconds': median,
                              'minimum_seconds': min(seconds), 'maximum_seconds': max(seconds),
                              'speedup_vs_serial': baseline / median})
    report = {'sizes': args.sizes, 'repeats': args.repeats, 'warmups_per_case': 1, 'case_order': commands,
              'code_sha256': source_fingerprints(),
              'compiler_flags': os.environ.get('CXXFLAGS', '-std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread'),
              'matrix': {'seed': 20261007, 'family': 'strict row-diagonal dominance', 'fingerprints_fnv1a64': fingerprints},
              'timing_scope': 'checked inverse: allocation, scaling, worker setup/distribution, solve, gather, one left residual and normalized residual; excludes MPI_Init/Finalize, process launch, input generation and right residual',
              'mpi_extra': 'includes collective diagnostic-error agreement; maximum elapsed rank duration',
              'limits': 'one host, small sample, no CPU pinning; no multi-node or general speedup claim',
              'environment': {'architecture': platform.machine(), 'os': platform.system(), 'python': platform.python_version(),
                              'logical_cpus': os.cpu_count(), 'compiler': subprocess.check_output([os.environ.get('CXX', 'c++'), '--version'], text=True).splitlines()[0],
                              'mpi_compiler': subprocess.check_output([os.environ.get('MPICXX', 'mpicxx'), '-show'], text=True).strip()},
              'summaries': summaries, 'max_two_sided_residual': max(float(r[k]) for r in rows for k in ('left_residual_inf', 'right_residual_inf')),
              'measurements_sha256': hashlib.sha256((args.output / 'measurements.csv').read_bytes()).hexdigest()}
    (args.output / 'metrics.json').write_text(json.dumps(report, indent=2) + '\n')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, 2, figsize=(11, 4), layout='constrained', facecolor='#f5f1e8')
    colors = ['#172e3b', '#ad8462', '#cfb497', '#7c8090', '#447f92', '#a34e3a']
    for (backend, workers), color in zip(configurations, colors):
        selection = sorted((r for r in summaries if r['backend'] == backend and r['workers'] == workers), key=lambda r: r['n'])
        xs = [r['n'] for r in selection]
        ys = [r['median_seconds'] for r in selection]
        label = 'Serial' if backend == 'serial' else f'{backend.upper() if backend == "mpi" else "Threads"} ×{workers}'
        axes[0].plot(xs, ys, 'o-', color=color, label=label)
        axes[0].fill_between(xs, [r['minimum_seconds'] for r in selection], [r['maximum_seconds'] for r in selection], color=color, alpha=.12)
        axes[1].plot(xs, [r['speedup_vs_serial'] for r in selection], 'o-', color=color, label=label)
    axes[0].set(xlabel='Matrix dimension', ylabel='Checked inverse · seconds', yscale='log', title=f'Median of {args.repeats}; shading spans measured repeats')
    axes[1].axhline(1, color='#777', ls=':', lw=1)
    axes[1].set(xlabel='Matrix dimension', ylabel='Serial time / backend time', title='Speedup includes the same diagnostic work')
    axes[0].legend(frameon=False, fontsize=8)
    fig.savefig(args.output / 'comparison.png', dpi=170)
    plt.close(fig)
    print(f'PASS: {len(rows)} matched measurements, all two-sided residuals <= 1e-10', flush=True)


if __name__ == '__main__':
    main()
