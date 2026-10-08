import itertools
import random
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "projects/algorithms"))
from solutions import (colored_cells, count_ways, distribute_money, find_score,
                       max_subarray, maximize_greatness, repair_cars, root_count, split_num)


class AlgorithmTests(unittest.TestCase):
    def test_greatness_against_all_permutations(self):
        for n in range(1, 6):
            for nums in itertools.combinations_with_replacement(range(3), n):
                oracle = max(sum(a > b for a, b in zip(p, nums)) for p in set(itertools.permutations(nums)))
                self.assertEqual(maximize_greatness(list(nums)), oracle)
        self.assertEqual(maximize_greatness([]), 0)

    def test_money_against_positive_compositions(self):
        def allocations(total, count):
            if count == 1:
                if total > 0 and total != 4:
                    yield int(total == 8)
            else:
                for first in range(1, total):
                    if first != 4:
                        for rest in allocations(total-first, count-1):
                            yield int(first == 8) + rest
        for children in range(1, 5):
            for money in range(30):
                self.assertEqual(distribute_money(money, children), max(allocations(money, children), default=-1))

    def test_repair_against_time_scan(self):
        for ranks in itertools.product(range(1, 5), repeat=3):
            for cars in range(8):
                def capacity(t):
                    return sum(sum(r*k*k <= t for k in range(1, cars+1)) for r in ranks)
                oracle = next(t for t in range(min(ranks)*cars*cars+1) if capacity(t) >= cars)
                self.assertEqual(repair_cars(list(ranks), cars), oracle)

    def test_roots_against_each_root_bfs(self):
        rng = random.Random(99)
        for n in range(1, 25):
            for _ in range(10):
                edges = [(v, rng.randrange(v)) for v in range(1, n)]
                guesses = [e if rng.randrange(2) else e[::-1] for e in edges if rng.randrange(2)]
                k = rng.randrange(n+1)
                good = 0
                for root in range(n):
                    parents = {root: -1}
                    queue = [root]
                    for v in queue:
                        for a, b in edges:
                            if a == v and b not in parents:
                                parents[b] = v; queue.append(b)
                            if b == v and a not in parents:
                                parents[a] = v; queue.append(a)
                    good += sum(parents[b] == a for a, b in guesses) >= k
                self.assertEqual(root_count(edges, guesses, k), good)

    def test_long_chain(self):
        n = 10_001
        edges = [(i, i+1) for i in range(n-1)]
        self.assertEqual(root_count(edges, edges, n-1), 1)

    def test_intervals_against_assignments(self):
        rng = random.Random(99)
        for _ in range(120):
            ranges = [tuple(sorted((rng.randrange(8), rng.randrange(8)))) for _ in range(5)]
            good = 0
            for sides in itertools.product((0, 1), repeat=5):
                good += all(sides[i] == sides[j] or max(a, c) > min(b, d)
                            for i, (a, b) in enumerate(ranges) for j, (c, d) in enumerate(ranges))
            self.assertEqual(count_ways(ranges), good)

    def test_kadane_and_heap(self):
        rng = random.Random(100)
        for _ in range(200):
            nums = [rng.randrange(-9, 10) for _ in range(rng.randrange(1, 15))]
            self.assertEqual(max_subarray(nums), max(sum(nums[i:j]) for i in range(len(nums)) for j in range(i+1, len(nums)+1)))
            available = set(range(len(nums))); score = 0
            while available:
                i = min(available, key=lambda j: (nums[j], j))
                score += nums[i]; available -= {i-1, i, i+1}
            self.assertEqual(find_score(nums), score)

    def test_digits_and_diamond(self):
        self.assertEqual(split_num(4325), 59)
        self.assertEqual(split_num(1000), 1)
        rng = random.Random(2578)
        for number in [*range(10, 1000), *(rng.randrange(1000, 10**6) for _ in range(150))]:
            digits = str(number)
            # Every reordering of the digits, cut into two nonempty numbers.
            oracle = min(int("".join(p[:k])) + int("".join(p[k:]))
                         for p in set(itertools.permutations(digits)) for k in range(1, len(digits)))
            self.assertEqual(split_num(number), oracle, number)
        for n in range(1, 10):
            points = {(x, y) for x in range(-n, n) for y in range(-n, n) if abs(x)+abs(y)<n}
            self.assertEqual(colored_cells(n), len(points))


if __name__ == "__main__":
    unittest.main()
