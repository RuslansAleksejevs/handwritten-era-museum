[← Museum](../README.md) · [Original comments](../docs/marginalia.md)

# Written by hand: the originals

**This is the direct route to a few selected pieces of my surviving handwritten code.** The six selections below preserve the old source, including comments, spelling, whitespace and mistakes. They are substantial functions and complete notebook cells, not newly generated lookalikes. The modern implementations live beside them through the links in the right column.

| Original, unchanged | What to look for | Modern reconstruction |
| :-- | :-- | :-- |
| [Serial Gauss–Jordan solver](numerics/serial-solver.cpp) | A complete column-pivot elimination function, with the first step written out by hand. | [Checked serial/threaded solver](../projects/numerical-methods/include/solver.hpp) |
| [POSIX threads worker](numerics/pthreads-worker.cpp) | The argument structure and entire worker: one thread selects the pivot, workers update separate columns, and barriers divide the steps. | [Persistent workers and coordinated errors](../projects/numerical-methods/include/solver.hpp) |
| [MPI solver](numerics/mpi-solver.cpp) | Both complete functions: broadcasting a column and cyclic work assignment across ranks. | [Distributed column ownership and collective errors](../projects/numerical-methods/mpi/solver.cpp) |
| [Tree rerooting solution](algorithms/root-count.py) | The complete contest solution: two recursive traversals, a mutable set of correct guesses, and restoration when returning from a subtree. | [Iterative rerooting](../projects/algorithms/solutions.py) |
| [Random correlations](learning/random-correlations.ipynb) | Seven original code cells, both saved plots and my reaction to a pattern found in independent noise. | [Select a pair, then check it on untouched data](../projects/machine-learning/random-correlations/README.md) |
| [CIFAR convolutional network](learning/convnet.py) | A complete network class: two convolutions, pooling and a classifier; shape calculations remain in the comments. | [The same architecture and a new compact CNN, trained under one protocol](../projects/machine-learning/cnn/README.md) |

For a short visit, start with the [random-correlation notebook](learning/random-correlations.ipynb) and its [modern experiment](../projects/machine-learning/random-correlations/README.md), then inspect the [MPI worker](numerics/mpi-solver.cpp) and its [matched parallel experiment](../projects/numerical-methods/comparison/README.md). The [six-stop tour](../README.md#six-places-to-start) also includes the CNN, music and C++ chapters.

## What “unchanged” means

The C++ exhibits are contiguous byte-for-byte extracts of their source files. The Python files are exact decoded notebook code cells. The random-correlation notebook excerpt preserves the selected cell sources and saved outputs, with a new notebook wrapper and execution metadata removed. No imports, repairs or formatting were added. The [manifest](manifest.json) records locations and SHA-256 fingerprints; `make check` checks that these exhibits still match their recorded fingerprints. Exact repository provenance is also retained in the private research record.

These extracts keep their original dependencies and assumptions. They are not standalone supported programs: the C++ code relies on its old matrix/barrier infrastructure; the Python cell expects the contest-provided `List` type. Historical error handling and recursion limits remain as they were. For example, the old threads worker can leave other workers waiting at a barrier when a pivot is singular. Run the modern versions when you want the checked implementations.

The rerooting notebook credits a tutorial for the initial idea and describes developing the solution from there. The CNN assignment prescribed the sequence of layer types; the preserved implementation supplies its dimensions and forward pass. Its cell expects the notebook's `nn` and `F` imports.

The music chapter preserves the school and final team-project context; it does not reproduce the old team's code. The C++ belt exhibits are modern reconstructions, pending any rediscovery of my original submissions.
