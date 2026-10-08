[← Collection](../README.md) · [Run the work](reproduce.md)

# Current scope

This page describes the current implementations, their validation, and the next technical milestones in the collection.

| Chapter | Included and checked | Still outside this edition |
| :-- | :-- | :-- |
| Numerical methods | Serial, fixed-worker and distributed MPI column-pivot inversion; collective failure handling; residuals; LAPACK comparisons; bounded timing studies, matched serial/threads/MPI comparison | Multi-node performance, blocked kernels, large-scale or process-failure-tolerant execution |
| Machine learning | Selection of correlations from independent noise and a held-out check; linear model and derivative checks; historical/modern CNN; real car segmentation; pretrained/random/pixel transfer comparison; Fashion-MNIST autoencoder/PCA; time-ordered demand forecasts | Missing historical Delivery Club training data; broad benchmark claims and extensive uncertainty studies for the four new experiments |
| Music | Pinned Bach soprano corpus, family-level splits, GRU vs Markov over three seeds, rhythm/rests, copying audit, paired audio/MIDI examples | Polyphony, expressive performance, whole-piece form, human listening study |
| Algorithms | Selected studies in rerooting, greedy reasoning, monotone search, and dynamic programming; explanations and independent small-case oracles | Broader performance measurements beyond correctness and complexity checks |
| Visualizations | Portable oscillation/saddle plots and a frequency animation | Interactive parameter exploration |
| C++ belts | New solutions and local checks linked to all 189 recovered pages; async search, transport routes/maps, Mython, spreadsheet, ownership and concurrency studies; exact adaptations in coverage files | Official course census, external grader/timing acceptance, restoration of missing original buggy starters |

The MPI implementation is now checked on multiple processes on one host. Multi-node behavior has not been tested. Music results now cover 40 held-out chorales; the listening examples do not establish human preference or full compositional ability.

Historical excerpts and modern implementations are identified separately throughout the collection.

A separate source copy and a newly installed Python environment passed the [8 October 2026 macOS check](reproduce.md). The full ML/music reruns and saved CNN weights reproduced the recorded results. Linux and Open MPI remain untested; the check did not replace the host operating system or compiler.
