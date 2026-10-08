[← C++ belts](../../README.md) · [← White Belt](../white/README.md)

# Yellow Belt: 37 recovered tasks

These independent 2026 C++17 exhibits cover all **37 task pages** in the recovered Yellow Belt inventory. That includes four assignments whose deliverable is a **test suite**, both decomposition exercises, and the final event database with conditions. These are modern implementations, not historical personal submissions.

[Coverage and exact sources](coverage.json) maps every page to code and tests. The count describes the recovered archive; it is not a claim about every official course edition or the external grader.

## Build and run

From the repository root:

```sh
python3 projects/cpp-belts/course/yellow/check.py --build /tmp/museum-yellow
python3 projects/cpp-belts/course/yellow/check.py --sanitize
printf '3\n' | /tmp/museum-yellow/yellow permutations
printf 'Add 2000-1-1 team meeting\nFind event == "team meeting"\n' | /tmp/museum-yellow/yellow database
```

Only a C++17 compiler and Python's standard library are needed. Set `CXX` to select a compiler. Builds are temporary unless `--build` is given; there are no network calls or external jobs.

| Executable command | Subject |
| :-- | :-- |
| `yellow matrix` | Integer matrix addition, bounds and shape contracts |
| `yellow temperature`, `yellow blocks` | Wide accumulators; maximum block total exceeds signed 64-bit range |
| `yellow buses` or `yellow-buses` | Queries, responses and storage compiled as separate translation units |
| `yellow permutations` | Descending lexicographic permutations |
| `yellow budget` | Inclusive range earnings, Gregorian leap years and income queries |
| `yellow budget-prefix` | Offline earnings with prefix sums and constant-time range queries |
| `yellow arithmetic`, `yellow arithmetic-minimal` | Expression construction with required or minimal parentheses |
| `yellow figures` | Polymorphic geometry; the course's specified π = 3.14 |
| `yellow database` or `yellow-events` | Event conditions, insertion order, removal and last-event queries |

APIs live in `yellow`, with nested namespaces where the course reuses names such as `Person` and `MergeSort`. The collection reuses its own [White Belt implementation](../white/README.md) where the same contract occurs again.

## Implementation map

- [exercises.hpp](exercises.hpp): matrices, region comparisons, task transitions, recursively squared containers, strict references, iterator exercises, stable binary and ternary merge sorts, nearest/prefix searches and demographic partitions. Name lookup reuses the binary search in [person.hpp](../white/person.hpp).
- [contract_tests.hpp](contract_tests.hpp): generic suites for root counts, name history, rational normalization and palindrome behavior. The test executable accepts correct implementations and rejects representative faulty candidates. These assignments are implemented as tests, not silently counted as ordinary solutions.
- [sum_reverse_sort.cpp](sum_reverse_sort.cpp), [phone_number.cpp](phone_number.cpp), [rectangle.h](rectangle.h): separate declarations and definitions, linked in the checks.
- [buses/](buses/): query parsing, typed response formatting and a manager in the requested file boundaries. The pinned driver was inspected to check the call interface; the implementation is new.
- [budget.hpp](budget.hpp): integer day ordinals independent of timezone/DST. The range version is bounded to the course's small query load; the offline version preprocesses all earnings once.
- [oop.hpp](oop.hpp): animals, notifier interfaces, figures and refactored role behavior. Notifier tests record calls locally; no SMS or email is sent.
- [events/](events/): date and database interfaces, polymorphic condition nodes, a new tokenizer and recursive-descent parser. Comparisons support `< <= > >= == !=`; `AND` binds more tightly than `OR`, and parentheses override precedence. Quoted events preserve spaces and reserved words. A date's event list preserves insertion order and rejects duplicates, including after delete/re-add.

## Checks and limits

[API checks](tests.cpp) cover 29 groups, including random task transitions against individual-task state, both merge sorts against an independent standard-library reference, extreme nearest-element distances, leap-year boundaries, all six comparison operators and condition precedence. [CLI checks](test_cli.py) exercise 11 contracts, a separate 700-operation event-store oracle, independent Python-calendar budget checks, 720 permutations and the maximum block sum of 10¹⁹. Both decomposed executables are linked and run separately.

Normal builds, all standalone headers and UndefinedBehaviorSanitizer passed locally with Apple Clang 17 on arm64 macOS. The broader fresh-environment audit remains deferred. The suite is evidence for the documented contracts, not external grader acceptance.

The standalone collection supplies types and helpers that the old grader would supply, including `Region`, `TaskStatus`, demographic records and median calculation. It uses namespaces to keep variants together. These are not verbatim grader upload files. Small tasks assume the valid input described by their statements; their CLI is not intended as a hardened input service. No third-party solution implementation is incorporated. The recovered driver's API informs compatibility, while the condition parser and tests were written afresh.
