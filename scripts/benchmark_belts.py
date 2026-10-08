"""Regenerate a bounded local timing sample; no speed threshold is a test."""
import json
import platform
import subprocess
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLAGS = "-std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread"
subprocess.run(["make", "-B", "build/benchmark-belts", "CXX=c++", f"CXXFLAGS={FLAGS}"],
               cwd=ROOT, check=True, timeout=60)
result = subprocess.run([str(ROOT / "build/benchmark-belts")], capture_output=True,
                        text=True, check=True, timeout=120)
data = json.loads(result.stdout)
data["environment"] = {
    "recorded_on": date.today().isoformat(),
    "system": platform.system(),
    "architecture": platform.machine(),
    "compiler": subprocess.check_output(["c++", "--version"], text=True).splitlines()[0],
    "flags": FLAGS,
}
data["scope"] = (
    "One machine, three timed samples after a warm-up. Query-only times; "
    "corpus generation/tokenization excluded for both methods. Search index construction "
    "reported separately; domain construction excluded. Search result vectors are compared "
    "exactly against a pretokenized full-scan/full-sort baseline outside timing. "
    "Every domain answer is compared against a linear suffix scan outside timing. "
    "No production, cross-machine or optimal-baseline claim."
)
path = ROOT / "projects/cpp-belts/results/benchmark.json"
path.parent.mkdir(exist_ok=True)
path.write_text(json.dumps(data, indent=2) + "\n")
print(path.relative_to(ROOT))
for prefix in ("search_rare", "search_common", "domains"):
    fast, slow = (data[prefix + suffix]["median_ms"] for suffix in ("_index", "_scan"))
    print(f"{prefix}: {fast:.3f} ms vs {slow:.3f} ms ({slow / fast:.1f}x)")
