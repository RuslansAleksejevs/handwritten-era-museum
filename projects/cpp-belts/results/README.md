[← C++ belts](../README.md) · [Raw timings](benchmark.json) · [Benchmark source](../tests/benchmark.cpp)

# What was checked and measured

## Correctness

The local C++17 run on 5 October 2026 passed:

- **24,000 search queries** on 200 seeded random document bases, compared exactly with a pretokenized full scan and full sort.
- **100,000 domain queries** on 400 seeded ban lists, compared with direct suffix checks.
- **40,000 concurrent search queries** from four readers while a writer replaced the index. Each result must belong wholly to one version.
- A deterministic blocked-input test: queries finish against the old index while the replacement waits for input.
- Input/output failures, ownership after stream destruction, empty and same-sized replacements, 50,000 tied documents, maximum assignment score, CLI examples and malformed input.
- The complete test executable under **UndefinedBehaviorSanitizer**, with recovery disabled.

These checks support the implementations and the stated contracts; randomized tests do not prove correctness. The explanations in each chapter state the invariants separately.

## Timing method

`python3 scripts/benchmark_belts.py` rebuilds with the recorded compiler flags and writes [benchmark.json](benchmark.json). It checks every answer before timing, warms up each workload, then records three samples and their median. Input generation and tokenization are outside both timed paths. Search index construction is reported separately. The optimized search includes result construction; neither path writes results to a terminal during timing.

The search corpus contains 8,000 documents of 25 words: 24 seeded words from a 1,024-word vocabulary and one word common to every document. There are two sets of 200 three-word queries: one uses only vocabulary words; the other always includes the common word. This exposes the cost of visiting all documents instead of showing only favorable sparse queries.

The domain workload uses 10,000 unrelated banned roots and 10,000 queries, half blocked and half allowed. The reference checks roots one by one. Index construction is outside the timing. The search baseline scans pretokenized documents and fully sorts matches; it is deliberately simple and is not the fastest alternative implementation.

| Local workload | Indexed approach, median | Simple scan, median |
| :-- | --: | --: |
| 200 sparse search queries | 0.513 ms | 491.645 ms |
| 200 queries containing the ubiquitous word | 6.611 ms | 372.184 ms |
| 10,000 domain queries | 1.085 ms | 78.806 ms |

Search index construction took 13.002 ms in this run. These query timings amortize that separate build cost; they should not be read as end-to-end startup timings.

The raw results record the machine architecture, compiler, flags, seeds, checksums and individual samples. Times are a local observation and depend on the machine, workload and load. No speed threshold is used as a correctness test; these are not production benchmarks.

## Sanitizer limits

UBSan passed. ThreadSanitizer compiled but its runtime crashed without a diagnostic on this machine; an empty TSan program was checked separately to distinguish this from an exhibit test failure. No TSan pass is claimed. AddressSanitizer was unavailable in the earlier environment check, where even an empty program timed out; it has not been re-certified here.
