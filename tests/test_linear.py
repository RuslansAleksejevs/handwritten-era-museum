import sys
import unittest
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "projects/machine-learning"))
from linear import finite_difference_error, hinge_loss_gradient, fit_linear


class LinearTests(unittest.TestCase):
    def test_gradient_matches_finite_differences(self):
        rng = np.random.default_rng(2)
        for samples in (1, 2, 7, 15):
            x = rng.normal(size=(samples, 4)); y = np.arange(samples) % 3
            w = rng.normal(scale=.01, size=(4, 3))
            for reg in (0, .1, 2):
                self.assertLess(finite_difference_error(x, y, w, reg), 1e-7)

    def test_repeating_batch_does_not_dilute_regularization(self):
        x = np.array([[1.,2.],[-1.,1.]]); y = np.array([0,1]); w = np.full((2,2),.1)
        a, da = hinge_loss_gradient(x,y,w,.3)
        b, db = hinge_loss_gradient(np.tile(x,(5,1)),np.tile(y,5),w,.3)
        self.assertAlmostEqual(a,b); np.testing.assert_allclose(da,db)

    def test_learns_separable_problem(self):
        x = np.array([[-2.,1.],[-1.,1.],[1.,1.],[2.,1.]])
        y = np.array([0,0,1,1]); w, losses = fit_linear(x,y,classes=2,steps=100)
        np.testing.assert_array_equal((x@w).argmax(1),y)
        self.assertLess(losses[-1],losses[0])
