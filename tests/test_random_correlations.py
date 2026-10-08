"""Independent pairwise oracles and a check that held-out data cannot select the pair."""
import importlib.util
from pathlib import Path
import statistics
import unittest

import numpy as np

PATH = Path(__file__).resolve().parents[1] / "projects/machine-learning/random-correlations/correlations.py"
SPEC = importlib.util.spec_from_file_location("museum_correlations", PATH)
correlations = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(correlations)


class CorrelationTests(unittest.TestCase):
    def test_block_search_matches_independent_pairwise_oracle(self):
        rng = np.random.default_rng(128)
        for variables in (2, 3, 7, 17):
            data = rng.normal(size=(13, variables))
            oracle = {(i, j): statistics.correlation(data[:, i], data[:, j])
                      for i in range(variables) for j in range(i+1, variables)}
            best = max(oracle, key=oracle.get)
            for block in (1, 4, 32):
                pair, value = correlations.strongest_pair(data, block)
                self.assertEqual(pair, best)
                self.assertAlmostEqual(value, oracle[best], places=13)

    def test_signed_search_excludes_diagonal_and_absolute_maximum(self):
        data = np.array([[1., -1.], [2., -2.], [3., -3.]])
        pair, value = correlations.strongest_pair(data)
        self.assertEqual(pair, (0, 1))
        self.assertAlmostEqual(value, -1.)
        data = np.column_stack((np.arange(6.), -np.arange(6.), [0., 2., 1., 4., 3., 5.]))
        self.assertEqual(correlations.strongest_pair(data)[0], (0, 2))

    def test_exact_ties_are_lexicographic(self):
        data = np.tile(np.arange(6.)[:, None], (1, 5))
        for block in (1, 2, 9):
            self.assertEqual(correlations.strongest_pair(data, block)[0], (0, 1))

    def test_holdout_changes_score_not_selection(self):
        rng = np.random.default_rng(77)
        discovery = rng.normal(size=(20, 8))
        held_out = rng.normal(size=(20, 8))
        first = correlations.compare_halves(discovery, held_out)
        i, j = first["pair_zero_based"]
        held_out[:, j] = -held_out[:, i]
        second = correlations.compare_halves(discovery, held_out)
        self.assertEqual(first["pair_zero_based"], second["pair_zero_based"])
        self.assertEqual(first["discovery_r"], second["discovery_r"])
        self.assertAlmostEqual(second["held_out_r"], -1.)
        self.assertGreater(abs(first["held_out_r"] - second["held_out_r"]), .1)

    def test_invalid_data(self):
        for data in (np.ones((4, 3)), [[1., 2.]], [1., 2.], [[1., np.nan], [2., 3.]]):
            with self.assertRaises(ValueError):
                correlations.strongest_pair(data)
        for block in (0, -1, 1.5, True):
            with self.assertRaises(ValueError):
                correlations.strongest_pair([[1., 2.], [3., 4.]], block)
        with self.assertRaises(ValueError):
            correlations.compare_halves(np.ones((4, 3)), np.ones((4, 2)))


if __name__ == "__main__":
    unittest.main()
