[← C++ belts](README.md) · [Materials](materials.md) · [New solutions](course/README.md)

# Restoration map

The recovered archive contains **189 candidate exercise leaf pages**. All now map to new implementations and local checks. This is one learner archive, not a count of official assignments or my historical submissions. Repeated problems share code; testing and decomposition pages remain separate entries.

| Belt | Located pages | Implementation and exact coverage |
| :-- | --: | :-- |
| White | 49 | [Chapter](course/white/README.md) · [Coverage](course/white/coverage.json) |
| Yellow | 37 | [Chapter](course/yellow/README.md) · [Coverage](course/yellow/coverage.json) |
| Red | 37 | [Chapter](course/red/README.md) · [Coverage](course/red/coverage.json) |
| Brown | 42 | [Chapter](course/brown/README.md) · [Coverage](course/brown/coverage.json) |
| Black | 24 | [Chapter](course/black/README.md) · [Coverage](course/black/coverage.json) |

[Full source index](exercises-index.json) · [Material fingerprints](materials-index.json).

`LOCAL_CONTRACT_TESTED` and `IMPLEMENTED_TESTED` mean a modern local implementation and executable checks exist. `ADAPTED_TESTED` means the core behavior is tested with an explicit changed boundary: examples include the JSON organization merger, reconstructed helper interfaces and sanitizer studies without the original buggy programs. No status means external-grader acceptance.

The [coverage checker](../../scripts/check_course_coverage.py) enforces unique source pages, verifies implementation/test paths and rejects unfinished entries. `make course-check` executes the five suites; `make course-sanitize` adds UBSan.

Course-specific timing limits and compatibility with every historical starter package have not been checked. Reconciliation across course editions remains open: the black-belt archive contains weeks 1–4 and 6, and the missing week-5 directory does not prove no material is missing. The museum audit on a fresh environment is deferred separately.

The [constants study](constants/README.md) is an extra experiment inspired by Matrosov's talk and is not included in the 189-page count.
