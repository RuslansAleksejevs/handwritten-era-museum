"""Subprocess timeouts make the old singular-matrix deadlock a test failure."""
import subprocess
import tempfile
from pathlib import Path

if not __debug__:
    raise SystemExit("assert-based checks need Python without -O or PYTHONOPTIMIZE")

subprocess.run(["build/test-solver"], check=True, timeout=45)
for threads in (1, 2, 4):
    subprocess.run(["build/inverse", "projects/numerical-methods/examples/pivot.txt", str(threads)],
                   check=True, timeout=5, capture_output=True)
    r = subprocess.run(["build/inverse", "projects/numerical-methods/examples/singular.txt", str(threads)],
                       timeout=5, capture_output=True, text=True)
    assert r.returncode == 1 and "singular" in r.stderr, r
with tempfile.TemporaryDirectory() as d:
    p = Path(d) / "matrix.txt"

    def run(text):
        p.write_text(text)
        return subprocess.run(["build/inverse", str(p)], timeout=5, capture_output=True, text=True)

    # The dimension is one whole token: "2.5" once became n=2 and an entry ".5".
    for bad in ("0", "-1", "2.5 1 0 1", "2e0 1 0 0 1"):
        r = run(bad)
        assert r.returncode == 1 and "n must be" in r.stderr, (bad, r.stderr)
    for bad in ("2 1 2", "1 nan", "1 inf", "1 1 extra"):
        assert run(bad).returncode == 1, bad
    r = run("2\n1 1e-310\n0 1\n")  # representable subnormal entry
    assert r.returncode == 0 and float(r.stdout.split()[1]) == -1e-310, r
print("PASS: CLI validation, subnormal input and singular regressions terminate within 5 seconds")
