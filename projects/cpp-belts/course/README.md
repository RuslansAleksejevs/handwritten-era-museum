[← C++ belts](../README.md) · [Exact source map](../curriculum.md)

# Five belts, reconstructed in 2026

These are new Codex implementations of the problems in a recovered learner archive, not my handwritten submissions. Every one of the **189 leaf pages** has an implementation/test mapping. Repeated problems share code; testing and decomposition tasks retain their own entries.

| Belt | Recovered pages | Start here |
| :-- | --: | :-- |
| [White](white/README.md) | 49 | Stateful commands, rational arithmetic and the event database |
| [Yellow](yellow/README.md) | 37 | Generic algorithms, test suites and condition parsing |
| [Red](red/README.md) | 37 | Lifetimes, concurrency and asynchronous search |
| [Brown](brown/README.md) | 42 | Indexes, segment-tree budgets and transport data |
| [Black](black/README.md) | 24 | Routing/maps, Mython, manual storage and spreadsheet formulas |

The code favors explicit ownership, standard algorithms and independent modules. Local checks include archived examples, deliberately broken implementations, brute-force references, lifetime counters and UBSan. They do not establish external-grader acceptance or a complete census of every course edition. Starter-dependent adaptations are named in each coverage file.

```sh
make course-check
make course-sanitize
```

These commands need C++17 and Python's standard library, without network access. `make check` includes the course checks. A whole-museum audit on a fresh environment is a separate, deferred task.

For each belt, `coverage.json` records the recovered page, implementation, tests and limitations. [The checker](../../../scripts/check_course_coverage.py) rejects missing pages, duplicates, unfinished statuses and missing local files. It checks the map's integrity; executable tests check behavior.
