[← Museum](../../README.md) · [Materials](materials.md) · [Source map](curriculum.md) · [Measurements](results/README.md)

# 06 / The C++ belts

*An excellent course from another era. Preserved carefully; probably not reopened before retirement.*

During my studies at MSU's Faculty of Mechanics and Mathematics, I worked through parts of **The Art of Modern C++ Development**, the five-belt specialization by Yandex and MIPT. Some of it came through our computer-work seminars with **Mikhail Lozhnikov**; some was independent. Mikhail Matrosov's explanations stayed with me — including how much there was to think about when declaring a constant.

I am keeping the course and these new solutions as a valued exhibit of unusually thoughtful C++ teaching. My own old solutions may still turn up. The code below was written afresh with Codex in 2026, drawing on the course problems and the principles I remember from them.

## Three things worth opening

| Exhibit | The interesting part | Open it |
| :-- | :-- | :-- |
| **A small search engine** | An inverted index, deterministic ranking, sparse score reuse, and immutable snapshots while the document base changes | [Explanation & runnable example](search/README.md) · [Implementation](search/search.cpp) |
| **Banned domains** | Turn suffix matching into a sorted, compact prefix problem; get the label boundaries right | [Explanation & runnable example](domains/README.md) · [Implementation](domains/domains.hpp) |
| **How to declare a constant** | Values, linkage, identity and lifetime across two translation units | [Executable study](constants/README.md) |

The approach is deliberately simple: values own their memory; borrowed views stay local; readers share immutable data; interfaces make lifetimes explicit. Standard containers and algorithms do the routine work. Each optimization comes with a reason, an independent correctness check and, where useful, a measurement. This is my reconstruction of what I valued in the teaching, not a claim that the instructors reviewed this code.

## Run the exhibits

From the repository root, with a C++17 compiler, Make and Python 3:

```sh
make belts-check constants-check
make belts-sanitize
python3 scripts/benchmark_belts.py
```

The search and domain checks include **124,000 comparisons with simple independent oracles**, **40,000 concurrent queries**, failed input, replacement of the database, ownership checks, CLI examples and malformed input. [Tests](tests/check.cpp) · [CLI checks](../../scripts/check_belts.py) · [Recorded results and limits](results/README.md).

## Five belts, with new solutions

The [course shelf](course/README.md) now maps **all 189 recovered leaf pages** to new implementations and local tests: [White](course/white/README.md), [Yellow](course/yellow/README.md), [Red](course/red/README.md), [Brown](course/brown/README.md), and [Black](course/black/README.md). These are pages from one learner archive, including repeated and testing tasks; the number is not an official course census.

Start with the asynchronous search service, the transport router and SVG map, the Mython interpreter, or the spreadsheet with formulas and structural edits. Missing starter interfaces and adapted exercises are marked in the [coverage map](curriculum.md). No course-grader acceptance is claimed.

```sh
make course-check
make course-sanitize
```

## The historical shelf

[All five belts and Matrosov's talks](materials.md) · [Located assignment pages](curriculum.md).

The constants study is an additional experiment inspired by a talk. It does not inflate the recovered-page count. The source map records which problems share an implementation and which use a reconstructed interface. The seminarist's [MSU teaching page](https://edu.math.msu.ru/1-kurs/computer-work/) independently records his computer-work seminars; the connection to my own learning is my recollection.

[← Back to the museum](../../README.md) · [Start with the search engine →](search/README.md)
