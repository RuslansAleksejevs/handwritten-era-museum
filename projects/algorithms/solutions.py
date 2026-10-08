"""Selected problems from the author's Biweekly 99/100 notes, rebuilt in 2026.

Independent brute-force oracles live in tests/test_algorithms.py.
"""
from collections import Counter
from heapq import heapify, heappop
from math import isqrt


def split_num(number: int) -> int:
    if number < 0:
        raise ValueError("number must be nonnegative")
    digits = sorted(str(number))
    return int("".join(digits[::2]) or "0") + int("".join(digits[1::2]) or "0")


def colored_cells(n: int) -> int:
    if n < 1:
        raise ValueError("n must be positive")
    return 1 + 2 * n * (n - 1)


def count_ways(ranges: list[tuple[int, int]], modulus: int = 1_000_000_007) -> int:
    if modulus < 1 or any(a > b for a, b in ranges):
        raise ValueError("invalid range or modulus")
    groups = 0
    right = None
    for a, b in sorted(ranges):
        if right is None or a > right:
            groups += 1
            right = b
        else:
            right = max(right, b)
    return pow(2, groups, modulus)


def root_count(edges: list[tuple[int, int]], guesses: list[tuple[int, int]], k: int) -> int:
    """Iterative rerooting on a tree whose vertex labels are 0..len(edges)."""
    n = len(edges) + 1
    graph = [[] for _ in range(n)]
    for a, b in edges:
        if not (0 <= a < n and 0 <= b < n) or a == b:
            raise ValueError("edges must form a labelled tree")
        graph[a].append(b)
        graph[b].append(a)
    directed = Counter(guesses)
    edge_set = {tuple(sorted((a, b))) for a, b in edges}
    if any(tuple(sorted(g)) not in edge_set for g in directed):
        raise ValueError("each guess must describe an edge")
    parent = [-2] * n
    parent[0] = -1
    order = [0]
    for v in order:
        for child in graph[v]:
            if child == parent[v]:
                continue
            if parent[child] != -2:
                raise ValueError("edges must form a tree")
            parent[child] = v
            order.append(child)
    if len(order) != n:
        raise ValueError("tree is disconnected")
    scores = [0] * n
    scores[0] = sum(directed[parent[v], v] for v in order[1:])
    for v in order[1:]:
        p = parent[v]
        scores[v] = scores[p] - directed[p, v] + directed[v, p]
    return sum(score >= k for score in scores)


def distribute_money(money: int, children: int) -> int:
    if money < 0 or children < 1:
        raise ValueError("money must be nonnegative and children positive")
    if money < children:
        return -1
    extra = money - children
    eights = min(extra // 7, children)
    remaining = extra - 7 * eights
    others = children - eights
    if others == 0 and remaining:
        return eights - 1
    if others == 1 and remaining == 3:
        return eights - 1
    return eights


def maximize_greatness(nums: list[int]) -> int:
    ordered = sorted(nums)
    matched = 0
    for candidate in ordered:
        if candidate > ordered[matched]:
            matched += 1
    return matched


def find_score(nums: list[int]) -> int:
    heap = [(value, i) for i, value in enumerate(nums)]
    heapify(heap)
    marked = [False] * len(nums)
    score = 0
    while heap:
        value, i = heappop(heap)
        if marked[i]:
            continue
        score += value
        for j in range(max(0, i-1), min(len(nums), i+2)):
            marked[j] = True
    return score


def repair_cars(ranks: list[int], cars: int) -> int:
    if not ranks or min(ranks) < 1 or cars < 0:
        raise ValueError("positive ranks and a nonnegative car count required")
    low, high = 0, min(ranks) * cars * cars
    while low < high:
        mid = (low + high) // 2
        if sum(isqrt(mid // rank) for rank in ranks) >= cars:
            high = mid
        else:
            low = mid + 1
    return low


def max_subarray(nums: list[int]) -> int:
    if not nums:
        raise ValueError("a nonempty sequence is required")
    best = running = nums[0]
    for i in range(1, len(nums)):
        value = nums[i]
        running = max(value, running + value)
        best = max(best, running)
    return best
