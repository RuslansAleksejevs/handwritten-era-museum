# Red Belt: containers, ownership and concurrency

Independent C++17 reconstructions of all **37 leaf statement pages** in the pinned
learner archive. These are new implementations, not recovered historical submissions.
The [coverage map](coverage.json) identifies each exact source page, implementation,
checks and limits. It is an inventory of that archive, not proof that it contained
every assignment ever used by the course.

From the museum root:

```sh
python3 projects/cpp-belts/course/red/check.py
python3 projects/cpp-belts/course/red/check.py --sanitize
./build/course-red/red express
./build/course-red/red reading
./build/course-red/red booking
./build/course-red/red sportsmen
./build/course-red/red learner
./build/course-red/red search projects/cpp-belts/search/examples/documents.txt < projects/cpp-belts/search/examples/queries.txt
```

The first four CLI modes use the original count-and-command input format. Learner
reads word lists until EOF. Search takes its documents from a file and queries from
stdin. [Archived input/output examples](examples.json) run as part of `check.py`.
`--build-dir /absolute/path` keeps build products outside the repository. `CXX`
selects the compiler. No dependencies are downloaded and no background job remains
after the checks finish.

## What is implemented

- [Foundations](foundations.hpp): predicate maximum, logging/comparator/update
  macros, safe statement and unique-name macros, a resizable table, forward-iterator
  pagination, reference-based student comparison, learner vocabulary, fixed-capacity
  stack vector, and a vector with copy and move support.
- [Deque](deque.hpp): the requested two-vector representation, in its own header
  without the prohibited list/deque/set/map includes.
- [Algorithms and ownership](algorithms.hpp): FIFO object pool, pointer algorithms
  including overlapping reversed copy, singly linked list, string-interning
  translator, enum-indexed counters, editor, HTTP request statistics, move-only
  Josephus permutation, heavy-character grouping, sentence splitting, exact
  three-way merge sort, and a priority collection with newest-first ties.
- [Services](services.hpp): nearest reachable express endpoints, Fenwick-tree reading
  ranks, booking-window eviction with distinct-client accounting, and sportsmen
  insertion using stable list iterators.
- [Concurrency](concurrency.hpp): locked access handles, bucketed integer maps,
  parallel matrix sum and bounded batches for keyword counting.
- [Search part one](../../search/README.md): the existing independently checked
  immutable search index. [Part two](async_search.hpp) adds owned asynchronous jobs
  around that core and the original stream-taking method names.

`SimpleVector` allocates raw storage and constructs only live elements. Capacity
doubles on append; append first owns its argument, so `v.PushBack(v[0])` remains
valid when storage moves. Reallocation moves old elements, allowing move-only
values. If an element move throws, the old vector remains valid but some elements
may be moved from; a stronger guarantee is not claimed. The standalone linked
list and translator intentionally reject copying to avoid accidental shared
ownership or dangling views.

The editor holds a stable cursor in a linked list. The translator owns each distinct
string once and stores views into that stable pool. Forward and backward mappings
remember their own most recent pair, as the statement requests; adding a new pair
does not erase historical mappings in the opposite direction.

## Search part two: asynchronous lifetime contract

Construction loads the initial index synchronously. `UpdateDocumentBase` and
`AddQueriesStream` start background jobs and return. Each query reads a complete
immutable index; building a replacement does not block existing queries. Updates
publish in completion order. The test deliberately blocks an update's input and
requires an independent query to finish before releasing it.

Streams are borrowed: each job needs distinct input/output streams that remain
alive until `Wait()` or destruction completes. `Wait()` joins jobs and rethrows the
first worker exception after joining the rest. The destructor joins but cannot
report worker failures; callers needing error reporting should explicitly call
`Wait()`. Do not destroy the server while another thread submits work. No original
asynchronous grader or throughput score is claimed.

## Checks and limits

The C++ tests exercise move-only values, character-copy counts, overlapping memory,
empty cases, lifetime and boundary conditions, 3,000 express operations against a
naive oracle, 5,000 reading operations against a naive oracle, 40,000 concurrent
increments, and the asynchronous search progress test. The CLI runner checks four
archived examples and a 100,000-booking live window. UBSan can run the same suite.

The archive does not supply the logger starter tests that define its exact prefix.
This version writes `file:line: message`, with file and line independently optional;
that local formatting choice is explicitly recorded as an adaptation. Public
symbols live under `museum::red` to coexist with other course versions. The
original upload packaging, private graders, hardware-dependent time limits and the
9000×9000 matrix-sum target of 15 ms have not been verified. A local functional pass
must not be read as historical course completion or original-grader acceptance.
