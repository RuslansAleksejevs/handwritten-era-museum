"""Reject incomplete or incomparable benchmark measurements before plotting."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from compare_parallel import checked_rows

HEADER = 'backend,workers,n,trial,matrix_fingerprint,checked_seconds,left_residual_inf,right_residual_inf,relative_residual\n'


class ComparisonTests(unittest.TestCase):
    def test_valid_and_missing_repeats(self):
        text = HEADER + 'serial,1,4,0,abc,0.1,1e-15,2e-15,1e-16\n'
        self.assertEqual(len(checked_rows(text, 'serial', 1, 4, 1)), 1)
        with self.assertRaises(ValueError): checked_rows(text, 'serial', 1, 4, 2)

    def test_wrong_identity_nan_and_failed_inverse_rejected(self):
        for row in ['mpi,2,4,0,abc,0.1,0,0,0',
                    'serial,1,4,0,abc,nan,0,0,0',
                    'serial,1,4,0,abc,0.1,0,0.1,0']:
            with self.assertRaises(ValueError): checked_rows(HEADER + row + '\n', 'serial', 1, 4, 1)


if __name__ == '__main__':
    unittest.main()
