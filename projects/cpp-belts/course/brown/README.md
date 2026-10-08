# Brown Belt: indexes, lifetime and service boundaries

This directory maps all **42 leaf statement pages** of the pinned learner archive.
It contains independent modern implementations, plus explicit reuse of the Red
Belt cores for repeated assignments. [Coverage](coverage.json) records exact source
paths, code, checks and adaptations. Transport routing part E is integrated through
[the Black Belt transport extension](../black/transport.hpp); its checks belong to
that extension.

From the museum root:

```sh
python3 projects/cpp-belts/course/brown/check.py
python3 projects/cpp-belts/course/brown/check.py --sanitize
./build/course-brown/brown budget-home
./build/course-brown/brown budget
./build/course-brown/brown transport-ab
./build/course-brown/brown transport-c
./build/course-brown/brown transport-json
./build/course-brown/brown demographics
./build/course-brown/brown persons
./build/course-brown/brown domains
```

Each mode reads the statement's stdin format. `budget-home` uses the fixed 13% tax;
`budget` accepts an explicit tax rate and spending. `transport-ab` and `transport-c`
use text, while `transport-json` implements part D. Reading, express and hotel
booking reuse the [Red CLI](../red/README.md). Additional `xml-json` and `json-xml`
modes demonstrate the local conversion library; `json-xml` uses root `expenses`.
`--build-dir /absolute/path` and `CXX` control local builds. No downloads or cloud
runs are part of this check.

## Implementations

- [Data structures](data.hpp): fixed-bucket hash set, binary-tree successor,
  composite hashers, stable records with three secondary indexes, once-initialized
  lazy values, const access to bucketed hash maps, and coefficient proxies for
  polynomials. Priority collections, synchronized values and object pools reuse
  the corresponding tested Red implementations.
- [Formats](formats.hpp) and [JSON](json.hpp): spending loaders and conversions,
  INI sections, aggregation classes and a standalone JSON parser/writer. JSON
  handles escapes and surrogate pairs, rejects duplicate keys and invalid number
  syntax, bounds nesting to 256, and represents numbers as finite doubles. `AsInt`
  validates integral range. Raw UTF-8 validation and arbitrary-precision numbers
  are outside its scope.
- [Ownership](ownership.hpp) and [UniquePtr](unique_ptr.hpp): move-only bookings,
  polymorphic zoo, expression trees, an owning email pipeline, and thread-safe LRU
  cache. `UniquePtr` neither uses `std::unique_ptr` nor includes `<memory>`.
- [Graphics](graphics.hpp): double dispatch without RTTI and texture-sharing shapes
  with clipping and cloning.
- [Services](services.hpp): independent demographic indexes, seven median-age
  groups, task-status transitions, domain wrappers and HTTP response construction.
- [Budget](budget.hpp): a lazy segment tree applies earned-income affine updates
  and spending additions separately. Every inclusive date-range operation is
  O(log D), with D = 36,525 days. Taxes affect income already present, not future
  earnings or expenses. Calendar arithmetic is independent of local time zones.
- [Transport](transport.hpp): definitions in either order, return trips, unique
  stops, geographic lengths, directional road lengths with reverse fallback,
  sorted stop-to-bus indexes, and JSON part-D responses. Routing lives in the
  shared Black extension rather than a second implementation here.

The cache serializes unpacking under its mutex. It retains at most the configured
number of content bytes; an oversized book clears the cache and is returned without
being stored. Books already held by callers remain alive after eviction. This
implements thread safety and LRU behavior without claiming parallel decompression.
A booking provider must outlive its bookings and its completion callback must not
throw. Concurrent-map snapshots lock one bucket at a time; they are thread-safe,
not a transaction across all buckets.

## What the archive cannot establish

Some statements refer to starter files absent from the local statement snapshot.
Those rows are marked **ADAPTED_TESTED**, rather than treated as original submissions:

- XML/JSON library-integration and refactoring tasks use new local document types.
  The XML library supports only the spending dialect in the statement.
- Aggregators use `Process(int)` and `Get() -> optional<double>`; empty sum is zero,
  other empty aggregates are absent, and mode ties choose the smaller number.
  Original starter behavior and namespace packaging have not been verified.
- Collision dispatch uses local geometric types and `long double` arithmetic,
  rather than the omitted integer-only geometry library. Texture ellipses use
  pixel centers; equivalence to the omitted ellipse helper is unverified.
- The comment-server statement omits request-body parsing and its captcha text.
  HTTP response formatting follows the visible contract. The reconstructed server
  explicitly uses bodies `user_id comment` and `user_id 42`, with challenge
  `What is 6 * 7?`; compatibility with the original hidden wire protocol is not
  claimed. Repeating header names is supported, with one computed Content-Length.
- Zoo, cache and RAII booking interfaces are reconstructed from visible descriptions.
  The booking ID is an integer and completion is `CancelOrComplete(const Booking&)`.
- The six original buggy demographic programs and buggy domain helpers are absent.
  Local tests cover their described boundary categories; passing the original
  mutation grader is unverified. Legacy budget reuses the modern core rather than
  pretending to patch an unavailable starter.

Public symbols use `museum::brown` (and nested namespaces) to coexist with other
chapters. This is a museum library with explicit adapters, not a bundle certified
for direct upload to an old grader.

## Verification

The C++ suite checks hash collisions, index cleanup and callback stopping, const
access, 40,000 concurrent map updates, lazy initialization/retry, polynomial proxy
reads and assignments, JSON failures and Unicode, XML roundtrips, ownership,
cache eviction/concurrent hits, all 16 dispatch combinations for symmetry,
texture clipping/lifetime, demographic groups, task transitions, domain boundaries,
HTTP formatting, and 5,000 budget operations against a day-by-day oracle.

The CLI checks [seven archived input/output examples](examples.json), including
transport AB/C/D, domain input/output and a 100,000-command budget workload. The
same checks can run under UBSan and `-fno-rtti`. Original hidden tests, hardware time
limits, missing starter mutations and external grader acceptance are not claimed.
