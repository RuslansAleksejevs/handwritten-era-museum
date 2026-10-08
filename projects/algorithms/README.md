[← Collection](../../README.md) · [Previous: music](../music-generation/README.md) · [Next: visualizations →](../visualizations/README.md)

# 04 / Thinking in algorithms

*Selected studies in rerooting, greedy reasoning, binary search, and dynamic programming.*

**March 2023 · LeetCode Biweekly Contests [99](https://leetcode.com/contest/biweekly-contest-99/) and [100](https://leetcode.com/contest/biweekly-contest-100/).** The contests took place on **4 March** and **18 March**. I kept notebooks to work through the ideas, revisit solutions, and leave myself reminders. The current chapter develops selected examples from those notes and a separate maximum-subarray exercise. [Dates and sources](../../docs/timeline.md).

The central question is why an algorithm works: what remains invariant, which choice can safely be made, and how to check the result independently. A short [historical rerooting excerpt](history.md) connects the modern treatment to my original reasoning about restoring traversal state.

This chapter develops a selection from my contest and algorithm work. Start with the rerooting argument, compare it with monotone search, then explore the greedy and maximum-subarray examples. The modern implementations are in [one readable module](solutions.py); [tests](../../tests/test_algorithms.py) compare them with slower independent references.

## Start with the two deeper studies

- **Rerooting:** count valid roots in linear time by updating only the edge whose direction changes. The historical notebook connects this study to my post-contest work.
- **Binary search on the answer:** reduce a scheduling question to a monotone feasibility test, with integer arithmetic at the boundary.

The contest dates locate the source material. This chapter presents algorithmic studies developed from those notebooks; it does not present the modern implementations as timed contest submissions.

## The map

| Function | Why it works | Time / extra space |
| :-- | :-- | :-- |
| `root_count` | Moving the root across an edge reverses only that edge's parent relation. | O(n+g) / O(n+g) |
| `repair_cars` | The number repairable by time t is monotone; binary-search the first feasible time using integer square roots. | O(m log T) / O(1) |
| `count_ways` | Overlapping closed intervals must stay together. Each merged component independently chooses one of two groups. | O(n log n) / O(n) |
| `max_subarray` | The best subarray ending here either extends the previous one or starts here. | O(n) / O(1) |
| `maximize_greatness` | In sorted order, use the smallest available value that beats the smallest unmatched value. | O(n log n) / O(n) |
| `find_score` | A heap ordered by (value, index) implements the selection rule; lazily skip marked entries. | O(n log n) / O(n) |
| `split_num` | Give the smallest digits the largest place values across two balanced-length numbers. Leading zeroes are harmless. | O(d log d) / O(d) |
| `colored_cells` | Layer k adds 4(k−1) cells; sum the arithmetic progression. | O(1) / O(1) |
| `distribute_money` | Give everyone one dollar, buy as many upgrades to eight as possible, then repair the two exceptional remainders. | O(1) / O(1) |

Here `d` is the digit count, `g` the number of guesses, `m` the number of mechanics, and `T = min(rank) × cars²`. Bounds use the usual word-arithmetic model; arbitrary-precision integer operations have an additional bit cost.

## Two useful invariants

**Rerooting.** Start with root 0 and count correct directed guesses. If a child `v` becomes the root in place of its parent `p`,

```text
score[v] = score[p] − guesses(p,v) + guesses(v,p).
```

Every other edge keeps its direction. An iterative parent traversal visits each edge a constant number of times and avoids Python recursion depth on a long chain. Duplicate guesses are counted with multiplicity; the original contest inputs used unique guesses.

**Repair time.** A mechanic with rank `r` repairs `floor(sqrt(t/r))` cars by time `t`. Integer arithmetic avoids a floating-point rounding error near a perfect square. The least-ranked mechanic doing everything supplies a feasible upper bound. At each iteration the first feasible time remains inside the search interval.

## Greedy details

For digit splitting, swapping a smaller digit into a larger place value never increases the sum. Balanced lengths minimize the sorted multiset of required place values. For greatness, assigning the least sufficient candidate leaves every larger candidate available for a harder match; any optimal matching can exchange that choice without losing a pair.

For money distribution, after reserving one dollar per child, each eight-dollar child costs seven extra. If everyone has eight but money remains, one must absorb it and cease to count. If exactly one child remains and would get four, another eight must be sacrificed. These are the only obstructions.

Closed intervals that merely touch also overlap. For the score problem, ties are resolved by original index. Kadane is defined on a nonempty sequence, so an all-negative input returns its largest element, not zero.

## Evidence and use

Tests enumerate small permutations, money allocations, and interval assignments; scan repair times; compare every possible root of random small trees; compare Kadane with all subarrays; and check a **10,001-vertex chain**.

```sh
python -m unittest discover -s tests -p test_algorithms.py -v
```

These bounded oracles complement the invariants above with independent regression checks.

[← Back to the map](../../README.md) · [Continue to visualizations →](../visualizations/README.md)
