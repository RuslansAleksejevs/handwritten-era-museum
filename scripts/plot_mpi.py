"""Render the recorded MPI sample without rerunning the experiment."""
import json
import os
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "museum-matplotlib-cache"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import NullLocator

DESTINATION = Path(__file__).resolve().parents[1] / "projects/numerical-methods/mpi/results"
rows = json.loads((DESTINATION / "benchmark.json").read_text())["runs"]
fig, axes = plt.subplots(1, 2, figsize=(10, 4.1), constrained_layout=True)
for ranks in (1, 2, 4):
    selected = [row for row in rows if row["ranks"] == ranks]
    for axis, metric in zip(axes, ("median_elimination_seconds", "median_distributed_seconds")):
        axis.plot([row["n"] for row in selected], [1000 * row[metric] for row in selected],
                  marker="o", label=f"{ranks} process" + ("es" if ranks > 1 else ""))
for axis, title in zip(axes, ("Elimination + communication", "Scatter → inverse → gather")):
    axis.set(yscale="log", xlabel="Matrix dimension", ylabel="Median time (ms)", title=title)
    axis.set_xscale("log", base=2)
    axis.set_xticks([64, 128, 256, 512], [64, 128, 256, 512])
    axis.xaxis.set_minor_locator(NullLocator())
    axis.grid(True, which="both", alpha=.2)
    axis.legend(frameon=False)
fig.suptitle("MPI on one machine · 3 samples after a warm-up")
fig.savefig(DESTINATION / "benchmark.png", dpi=180)
plt.close(fig)
