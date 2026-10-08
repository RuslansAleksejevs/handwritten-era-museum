"""Vectorized multiclass hinge loss with a gradient of the same objective."""
import numpy as np


def hinge_loss_gradient(x, y, weights, regularization=0.0):
    x, y, weights = np.asarray(x, float), np.asarray(y), np.asarray(weights, float)
    if x.ndim != 2 or weights.ndim != 2 or len(x) == 0 or x.shape[1] != weights.shape[0]:
        raise ValueError("incompatible or empty arrays")
    if y.shape != (len(x),) or not np.issubdtype(y.dtype, np.integer):
        raise ValueError("one integer class label per example is required")
    if np.any(y < 0) or np.any(y >= weights.shape[1]) or regularization < 0:
        raise ValueError("invalid label or regularization")
    if not np.isfinite(x).all() or not np.isfinite(weights).all() or not np.isfinite(regularization):
        raise ValueError("inputs must be finite")
    scores = x @ weights
    margins = np.maximum(0.0, scores - scores[np.arange(len(x)), y, None] + 1.0)
    margins[np.arange(len(x)), y] = 0
    active = (margins > 0).astype(float)
    active[np.arange(len(x)), y] = -active.sum(axis=1)
    loss = margins.sum() / len(x) + regularization * np.square(weights).sum()
    gradient = x.T @ active / len(x) + 2 * regularization * weights
    return float(loss), gradient


def fit_linear(x, y, *, classes, regularization=0.001, steps=600, learning_rate=0.03):
    weights = np.zeros((x.shape[1], classes))
    losses = []
    for step in range(steps):
        loss, gradient = hinge_loss_gradient(x, y, weights, regularization)
        weights -= learning_rate / (1 + step / steps) * gradient
        losses.append(loss)
    return weights, losses


def finite_difference_error(x, y, weights, regularization, epsilon=1e-6):
    _, analytic = hinge_loss_gradient(x, y, weights, regularization)
    numeric = np.zeros_like(weights)
    for index in np.ndindex(weights.shape):
        plus, minus = weights.copy(), weights.copy()
        plus[index] += epsilon; minus[index] -= epsilon
        numeric[index] = (hinge_loss_gradient(x, y, plus, regularization)[0]
                          - hinge_loss_gradient(x, y, minus, regularization)[0]) / (2 * epsilon)
    return float(np.max(np.abs(numeric-analytic)))
