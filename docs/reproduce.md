[← Collection](../README.md) · [Origins](origins.md) · [Current scope](scope.md)

# Run the collection

Use Python 3.12 or newer, a C++17 compiler, and Make. Run these commands from the repository root. No GPU, API key or remote notebook is required. The music experiment downloads a pinned 178 KiB score archive; the test suite stays offline.

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
make check
```

The default dependency list specifies compatible version ranges. For the tested CPython 3.14.6 / macOS arm64 environment, install [requirements-lock.txt](../requirements-lock.txt) instead. Its 38 exact dependencies were installed into a new virtual environment on 8 October 2026; `pip check` and `make check` passed. This lock is for that platform, not a tested Linux lock. The [original environment record](environment.json) and [fresh-run report](reproducibility.json) distinguish the recorded runs from the later check.

The fresh check used a separate source copy without Git history or old build products, on the same Mac. It repeated the linear model, random correlations, plotting gallery, four further ML studies, and the complete music experiment. Numerical reports matched after excluding elapsed training time and source metadata; plots matched pixel for pixel, and music MIDI/MP3 files matched byte for byte. The seven retrained checkpoint files also matched. All nine saved CNN models reproduced their stored predictions and scores; CNN training was not repeated. MPICH checks and MPI UBSan passed. Linux, Open MPI and a fresh operating-system installation remain untested.

## Recreate the artifacts

```sh
python scripts/numerical_experiment.py
python projects/machine-learning/run.py
python projects/music-generation/run.py --cache /tmp/museum-music
python projects/visualizations/render.py
```

Each command writes into the corresponding chapter's `results/` directory. Music scores, weights and WAV audio stay in the external cache; install FFmpeg to also recreate the committed MP3s. Seeds and experimental scope are saved with the metrics. Numerical timings depend strongly on hardware and load; floats may vary across BLAS libraries and platforms. Regenerating results deliberately changes those files.

`make check` compiles and tests C++, runs Python tests, and checks repository-local Markdown/image links. C++ subprocesses have timeouts so a threading regression fails instead of hanging indefinitely. Python checks compare algorithms against brute-force references and gradients against finite differences.

## Minimal numerical run

```sh
make
./build/inverse projects/numerical-methods/examples/pivot.txt 4
```

The input is the matrix dimension followed by row-major entries. The inverse is printed to standard output; residual diagnostics go to standard error. Invalid input and unresolved pivots return a nonzero exit code.

The included [CI configuration](../.github/workflows/check.yml) is manual-only. It has not been run on GitHub in this first local edition.

The C++ suite also passed UndefinedBehaviorSanitizer locally. AddressSanitizer could not be validated on this machine: even a separately compiled empty program timed out. No ASan pass is claimed.

## C++ belt exhibits

```sh
make belts-check constants-check
make belts-sanitize
make course-check
make course-sanitize
python3 scripts/benchmark_belts.py
```

These commands need only the C++17 compiler, Make and Python's standard library. The check targets run oracles and CLI checks; the sanitizer targets add UBSan. The benchmark script regenerates the bounded timing sample. [Methods and sanitizer limits](../projects/cpp-belts/results/README.md).

The course checks compile the five belts and exercise their CLI and library contracts. They also check that every recovered page has an implementation/test mapping. Black Belt's optional independent protobuf interoperability check needs the Python `protobuf` package (recorded version 7.36.2): `python projects/cpp-belts/course/black/check.py --protobuf`. The default checks use a fixed protobuf wire-format golden and malformed inputs without that dependency. C++ protobuf is not required.

## Patterns from pure noise

```sh
python projects/machine-learning/random-correlations/run.py --output /tmp/museum-correlations
```

This offline experiment recreates the old seeded draw, searches 12,497,500 pairs on the first half of the observations and checks the chosen pair on the second half. [Original code, results and lesson](../projects/machine-learning/random-correlations/README.md).

## Four more ML studies

Each new chapter has its own data provenance, download size, cache and invocation:
[segmentation](../projects/machine-learning/segmentation/README.md),
[transfer](../projects/machine-learning/transfer/README.md),
[autoencoders](../projects/machine-learning/autoencoder/README.md), and
[forecasting](../projects/machine-learning/forecasting/README.md).
Their full experiments were repeated in the fresh Python environment, including pretrained feature extraction. `make check` exercises synthetic regression cases without downloading data or retraining. Large archives and model weights remain outside Git; the [saved-weight manifest](weights-manifest.json) and [restoration instructions](weights.md) describe the prepared weight bundle. The [exact source used for the original recorded runs](../projects/machine-learning/recorded-source/README.md) is included independently of Git history.

## Distributed MPI inverse

Install MPICH or Open MPI with the `mpicxx` wrapper and `mpiexec` launcher. Then:

```sh
make mpi-check
make mpi-sanitize
python3 scripts/benchmark_mpi.py
```

`make mpi-check` checks 1, 2, 3, 4 and 6 processes and uses NumPy as an independent oracle. It is separate from `make check`, so installing MPI is not required to explore the other exhibits. The check fails if the compiler/launcher is unavailable; it does not silently skip distributed tests. [MPI setup, contracts and timing scope](../projects/numerical-methods/mpi/README.md).

## CIFAR-10 CNN and matched parallel comparison

```sh
python projects/machine-learning/cnn/run.py --cache /tmp/museum-cifar --download --output /tmp/cnn-run
python projects/machine-learning/cnn/verify.py --cache /tmp/museum-cifar --results /tmp/cnn-run
MPI_BUILD=/tmp/museum-mpi MPICXX=mpicxx MPIEXEC=mpiexec \
  python scripts/compare_parallel.py --output /tmp/museum-comparison
```

These studies require fresh output directories so recorded results are not overwritten. CNN training uses CPU and downloads the official 162 MiB CIFAR-10 archive; weights and per-example predictions stay in the external cache. [CNN protocol and data credit](../projects/machine-learning/cnn/README.md). The parallel study requires MPI and compares identical matrix inputs with matching diagnostics; [timing boundaries and interpretation](../projects/numerical-methods/comparison/README.md). Both are separate from the fast offline test suite.
