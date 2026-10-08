"""Select on discovery data; evaluate the frozen pair on untouched observations."""
from numbers import Integral

import numpy as np


def standardized_columns(values):
    """Center each column and give it unit Euclidean norm."""
    values = np.asarray(values, dtype=np.float64)
    if values.ndim != 2 or min(values.shape) < 2:
        raise ValueError("need at least two observations and two variables")
    if not np.isfinite(values).all():
        raise ValueError("observations must be finite")
    centered = values - values.mean(axis=0)
    norms = np.linalg.norm(centered, axis=0)
    if not np.isfinite(norms).all() or np.any(norms == 0):
        raise ValueError("every variable must have finite, nonzero variation")
    return centered / norms


def strongest_pair(discovery, block_size=256):
    """Largest signed Pearson r among distinct pairs, using discovery data only.

    Search in row blocks instead of storing a full variables-by-variables matrix.
    Exact ties use the first (i, j) in lexicographic order, with i < j.
    """
    if not isinstance(block_size, Integral) or isinstance(block_size, bool) or block_size < 1:
        raise ValueError("block_size must be a positive integer")
    normalized = standardized_columns(discovery)
    variables = normalized.shape[1]
    columns = np.arange(variables)
    best_pair, best_r = None, -np.inf
    for start in range(0, variables, block_size):
        end = min(start + block_size, variables)
        scores = normalized[:, start:end].T @ normalized
        # Exclude self-correlations and count each unordered pair only once.
        scores[columns[None, :] <= np.arange(start, end)[:, None]] = -np.inf
        row, col = np.unravel_index(np.argmax(scores), scores.shape)
        value = float(scores[row, col])
        if value > best_r:
            best_pair, best_r = (start + int(row), int(col)), value
    return best_pair, float(np.clip(best_r, -1, 1))


def compare_halves(discovery, held_out, block_size=256):
    """Keep the selected indices fixed when looking at the other half."""
    discovery = np.asarray(discovery, dtype=np.float64)
    held_out = np.asarray(held_out, dtype=np.float64)
    if (discovery.ndim != 2 or held_out.ndim != 2
            or discovery.shape[1] != held_out.shape[1]):
        raise ValueError("both halves must have the same variables")
    pair, discovery_r = strongest_pair(discovery, block_size)
    selected = standardized_columns(held_out[:, list(pair)])
    held_out_r = float(np.clip(selected[:, 0] @ selected[:, 1], -1, 1))
    return {"pair_zero_based": list(pair), "discovery_r": discovery_r, "held_out_r": held_out_r}


def experiment(seed=42):
    # Preserve the old notebook's np.random.seed(42) + np.random.normal stream,
    # without changing NumPy's process-wide random state.
    observations = np.random.RandomState(seed).normal(size=(40, 5000))
    discovery, held_out = observations[:20], observations[20:]
    result = compare_halves(discovery, held_out)
    pair = result["pair_zero_based"]
    return {
        "seed": seed,
        "generator": "NumPy RandomState (MT19937), independent N(0,1) entries",
        "variables": 5000,
        "discovery_observations": 20,
        "held_out_observations": 20,
        "distinct_pairs_searched": 5000 * 4999 // 2,
        "selection": "maximum signed Pearson correlation on the first half; i < j",
        **result,
        "selected_data": {
            "discovery": discovery[:, pair].tolist(),
            "held_out": held_out[:, pair].tolist(),
        },
    }
