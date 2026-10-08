[← C++ belts](../../README.md) · [Yellow Belt →](../yellow/README.md)

# White Belt: 49 recovered tasks

These are independent 2026 C++17 implementations of all **49 task pages** in the recovered White Belt inventory. They cover the small programs, reusable functions/classes, and the event-database project. They are modern museum exhibits, not recovered personal submissions or copies of another learner's solutions.

[Coverage and exact sources](coverage.json) maps every task page to its implementation, entry point and checks. This is complete coverage of this recovered inventory; it is not evidence that the archive contains every assignment from every course edition.

## Build and check

From the repository root:

```sh
python3 projects/cpp-belts/course/white/check.py --build /tmp/museum-white
python3 projects/cpp-belts/course/white/check.py --sanitize
printf '1/3 + 1/6\n' | /tmp/museum-white/white calculator
printf 'Add 2000-1-1 hello\nPrint\n' | /tmp/museum-white/white database
```

The runner needs Python's standard library and a C++17 compiler (`CXX` may select it). It uses temporary build directories by default. There are no downloads, network calls, external graders, or message deliveries.

The executable takes an exercise name and then reads that exercise's documented input. The four file exercises use `input.txt` in the working directory; `copy-file` writes `output.txt` there. Other commands use standard input/output.

| Commands | Study |
| :-- | :-- |
| `sum`, `min-string`, `equation`, `division`, `price` | Arithmetic, branches and real roots |
| `even`, `second-f`, `gcd`, `binary` | Loops and string/digit processing |
| `temperature`, `queue`, `months`, `anagrams` | Stateful sequences and counters |
| `capitals`, `buses`, `routes`, `route-sets`, `unique`, `synonyms` | Maps, sets, identity, insertion order |
| `abs-sort`, `case-sort` | Comparison rules |
| `copy`, `copy-file`, `precision`, `table`, `students` | Files, exact formatting and records |
| `calculator`, `database` | Rational arithmetic, exceptions and event storage |

## Reusable code

- [basics.hpp](basics.hpp): factorial, palindrome/filter, move/reverse, map values, sorted strings, reversible strings, tagged lecture fields, invertible functions and a time-server adapter.
- [person.hpp](person.hpp): historical name lookup and history formatting. The birth-aware variant is `white::birth::Person`; it has no default constructor. Ordinary lookup uses `map::upper_bound`.
- [rational.hpp](rational.hpp): normalized rational numbers, arithmetic, comparison and stream operators. Intermediates use 64-bit arithmetic; normalized results outside the declared `int` representation throw rather than overflow.
- [buses.hpp](buses.hpp): preserve route creation order for interchanges and alphabetical order for the full route listing.
- [date.hpp](date.hpp) and [database.hpp](database.hpp): signed date components, exact format diagnostics, duplicate suppression and chronological/lexicographic event ordering. Dates intentionally allow every day from 1 to 31, as the exercise specifies.

APIs live in namespace `white` so several exercise variants can coexist. `tests.cpp` contains complete executable usage examples. `TimeServer` calls a caller-supplied `white::AskTimeServer`; tests supply a fake and distinguish network errors from other exceptions.

## Evidence and boundaries

[API checks](tests.cpp) exercise 22 groups, including exhaustive binary-string palindrome cases, rational arithmetic/comparison oracles over small signed fractions, compile-time constructor restrictions, out-of-order name changes, date errors and cached-time failures. [CLI checks](test_cli.py) cover all 28 commands, formatting/error branches and a separate 500-operation event-store oracle. Ordinary builds, standalone header checks and UndefinedBehaviorSanitizer passed locally with Apple Clang 17 on arm64 macOS.

The recovered quadratic statement prints `fire` in one no-real-root example. The implementation follows its actual request: print no roots. Numerical functions and basic integer exercises otherwise use the statement's valid-input and representable-result assumptions. Small exercises are not general-purpose untrusted-input parsers.

Original grader acceptance has not been tested. Namespaces and local scaffolding make this a runnable collection, not a collection of unchanged submission archives. The broader fresh-environment reproduction audit remains deferred.
