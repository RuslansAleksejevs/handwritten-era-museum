"""Rerun the old random-correlation experiment and draw its held-out reveal."""
import argparse
import hashlib
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from correlations import experiment

HERE = Path(__file__).resolve().parent


def draw_comparison(result, output):
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.5), sharex=True, sharey=True)
    fig.patch.set_facecolor("#f5f1e8")
    values = np.array([result["selected_data"][key] for key in ("discovery", "held_out")])
    bound = float(np.ceil(np.abs(values).max() * 2) / 2 + .25)
    for ax, data, color, title, correlation in zip(
        axes, values, ("#a64d35", "#243c45"),
        ("The pair we searched for", "The same pair on new data"),
        (result["discovery_r"], result["held_out_r"]),
    ):
        ax.set_facecolor("#fcfaf5")
        ax.scatter(data[:, 0], data[:, 1], color=color, s=48, edgecolors="white", linewidths=.6)
        ax.axhline(0, color="#d9d1c3", linewidth=.8, zorder=0)
        ax.axvline(0, color="#d9d1c3", linewidth=.8, zorder=0)
        ax.set(xlim=(-bound, bound), ylim=(-bound, bound),
               xlabel="First selected variable", title=f"{title}\nr = {correlation:.3f}")
        ax.set_aspect("equal")
        ax.spines[["top", "right"]].set_visible(False)
    axes[0].set_ylabel("Second selected variable")
    fig.suptitle("A convincing pattern, made entirely of noise", fontsize=16, color="#213e46")
    fig.text(.5, .035, "12,497,500 pairs searched  ·  20 observations per panel  ·  identical axes",
             ha="center", fontsize=10, color="#53666a")
    fig.subplots_adjust(left=.075, right=.98, bottom=.19, top=.76, wspace=.15)
    fig.savefig(output, dpi=170, facecolor=fig.get_facecolor())
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", type=Path, default=HERE / "results")
    options = parser.parse_args()
    result = experiment(options.seed)
    result["code_sha256"] = {name: hashlib.sha256((HERE/name).read_bytes()).hexdigest()
                             for name in ("correlations.py", "run.py")}
    result["scope"] = "One fixed synthetic experiment, reconstructed in 2026; no fitted predictive model."
    result["numpy_version"] = np.__version__
    options.output.mkdir(parents=True, exist_ok=True)
    draw_comparison(result, options.output / "comparison.png")
    (options.output / "metrics.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"Pair {result['pair_zero_based']}: discovery r={result['discovery_r']:.6f}, "
          f"held-out r={result['held_out_r']:.6f}")


if __name__ == "__main__":
    main()
