[← C++ belts](../README.md) · [Code](search.cpp) · [Tests](../tests/check.cpp) · [Measurements](../results/README.md)

# A small search engine

**Source:** the Red Belt final project, [part one](https://github.com/ivtsylin/BeltsCpp/blob/87c0b9ba255c8716a559d1d56c87cd06a5701e5e/03-Red-Belt/06-Course-project/01-FinalProjectPart1/README.md) and [part two](https://github.com/ivtsylin/BeltsCpp/blob/87c0b9ba255c8716a559d1d56c87cd06a5701e5e/03-Red-Belt/06-Course-project/02-FinalProjectPart2/README.md). This is an independent 2026 implementation. It does not use the archive author's solution.

## The problem

Each input line is a document. A query scores a document by the total number of occurrences of its words. Repeating a word in the query repeats its contribution. Return at most five documents, ordered by decreasing score, then increasing document ID. Zero scores disappear; words must match completely.

For documents `a a`, `b`, and `a b`, the query `a b` returns:

```text
a b: {docid: 0, hitcount: 2} {docid: 2, hitcount: 2} {docid: 1, hitcount: 1}
```

## The implementation

The index stores one posting `(document ID, frequency)` per distinct word/document pair. Construction proceeds in document order, so repeated occurrences update the last posting. The finished dictionary is sorted once; queries find words with binary search and a borrowed `string_view`. The dictionary owns its strings. No query view survives a call.

A reader's workspace keeps a score array and the IDs actually touched. The next query clears only those IDs, avoiding a full array initialization on every query. First use, or a change in document count, initializes the array. `partial_sort` orders just the best five results. Ties use the document ID, making output deterministic.

`SearchServer` publishes a `shared_ptr<const Index>` using C++17 atomic shared-pointer operations. A query holds one snapshot until it finishes. A replacement is built separately and published only after a successful read. A reader can therefore continue while the writer waits for input; failure leaves the old index available. Retired versions are reclaimed after their last reader finishes.

## A deliberate interface choice

`AddQueriesStream` and `UpdateDocumentBase` are **synchronous**. Run them on caller-owned threads to overlap work. This makes input/output ownership visible and propagates exceptions to the calling thread, without hidden background jobs borrowing streams that may have gone out of scope.

This implements the first part's search behavior and adapts the second part's concurrent-update idea. It is **not a drop-in submission to the original asynchronous part-two grader**. The types live in `museum::search`; there is no course submission adapter.

Use one workspace and distinct streams per concurrent caller; join all callers before destroying the server or streams. Concurrent updates publish in completion order. Snapshot publication is thread-safe, not claimed to be lock-free. Input is case-sensitive, space-delimited text under the original lowercase-word contract. Tabs, Unicode normalization, persistence, web requests and relevance beyond word counts are outside this exhibit.

## Why it is correct

For each query token, its postings add exactly that token's frequency in every matching document. Summing those additions gives the specified score, including repeated tokens. Every positive score gets one touched entry. Selecting five entries with the score/ID ordering therefore gives precisely the required result. Each query reads one complete immutable index, so an update cannot mix document IDs or counts from different versions.

## Cost

Let `D` be the document count, `V` the dictionary size, `Q` the query token count, `P` the number of visited postings, and `M` the matched-document count. Ignoring word-comparison length, a warm query costs **O(Q log V + P + M log 5)**, plus clearing the previous query's matched IDs. First use or a size change costs an additional **O(D)**. Workspace memory is **O(D)** per active reader; the index stores the dictionary and distinct word/document pairs. Building it uses expected-linear hash accumulation followed by **O(V log V)** sorting.

The frequent-word case still visits most documents; the benchmark includes it. Multiple index versions may coexist while readers finish. Those are explicit costs of this design.

## Run it

```sh
make build/search
./build/search projects/cpp-belts/search/examples/documents.txt < projects/cpp-belts/search/examples/queries.txt
make belts-check
```

The tests compare 24,000 seeded queries with a full-scan/full-sort oracle. They also cover 50,000 tied documents, maximum assignment hit count, empty documents and queries, failed reads, failed writes, destroyed input streams, same-size and empty replacements, a deliberately blocked writer, and 40,000 queries during concurrent replacement. UBSan passed; see [sanitizer scope](../results/README.md).
