[← C++ belts](../README.md) · [Code](domains.hpp) · [Tests](../tests/check.cpp) · [Measurements](../results/README.md)

# Banned domains

**Source:** the [Brown Belt assignment](https://github.com/ivtsylin/BeltsCpp/blob/87c0b9ba255c8716a559d1d56c87cd06a5701e5e/04-Brown-Belt/05-Fifth-week/07-BannedDomains/README.md), revisited in the [Black Belt sanitizer exercise](https://github.com/ivtsylin/BeltsCpp/blob/87c0b9ba255c8716a559d1d56c87cd06a5701e5e/05-Black-Belt/01-First-week/01-Forbidden%20domains/README.md). This independent implementation solves the shared filtering problem. It is not a patch to the course's deliberately buggy starter code.

If `ya.ru` is forbidden, block `ya.ru` and `mail.ya.ru`, but allow `notya.ru`, `ya.rus` and `ru`. The boundary between labels is the important detail.

## Turn the problem around

Reverse each domain and append a dot:

```text
ya.ru       → ur.ay.
mail.ya.ru  → ur.ay.liam.
notya.ru    → ur.ayton.
```

A banned domain now matches a **prefix** ending at a label boundary. Sort the reversed roots, discard duplicates and roots already covered by a broader one, then use `upper_bound` for each query. Only the immediately preceding root can match.

Why just one predecessor? All strings with a given prefix occupy a contiguous lexicographic interval. Once covered roots are removed, another retained root cannot lie between a matching root and its extension. Appending the dot prevents a partial-label match from entering that interval.

The filter owns its strings and is immutable after construction. Query strings are borrowed only for the duration of a call; each query builds its own reversed key. The implementation uses standard containers and algorithms without stored pointers into caller-owned text.

For `B` supplied roots of maximum length `L`, construction costs **O(B L log B)** and storage **O(B L)**. A query costs **O(L log B)** and **O(L)** temporary space. Covered roots reduce both storage and lookup work. A trie is a reasonable alternative, but a sorted vector suffices here without per-node allocations or a custom allocator.

## Run it

```sh
make build/domains
./build/domains < projects/cpp-belts/domains/example.txt
```

Expected output:

```text
Bad
Bad
Bad
Bad
Bad
Good
Good
```

The CLI accepts the course's lowercase ASCII labels: up to 10,000 domains per list, at most 50 characters each. It rejects malformed labels, invalid counts, missing and trailing input. This is the teaching problem's domain grammar, not a browser/DNS hostname parser: internationalized names, digits, hyphens, ports and URLs are intentionally outside the contract.

The checks include **100,000 seeded comparisons** with a direct suffix oracle, redundant bans, empty sets, boundary counterexamples and malformed CLI input. The measured workload uses the course limit of 10,000 roots and 10,000 queries; every answer is independently checked before timing.
