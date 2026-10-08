"""Bounded C++ checks and end-to-end input/output checks for the belt exhibits."""
import subprocess
import tempfile
from pathlib import Path

if not __debug__:
    raise SystemExit("assert-based checks need Python without -O or PYTHONOPTIMIZE")

ROOT = Path(__file__).resolve().parents[1]


def run(name, data="", args=(), ok=True):
    result = subprocess.run([str(ROOT / "build" / name), *args], input=data,
                            text=True, capture_output=True, timeout=60)
    if (result.returncode == 0) != ok:
        raise AssertionError(f"{name}: exit {result.returncode}: {result.stderr}")
    return result.stdout


def main():
    print(run("test-belts"), end="")
    assert run("domains", "4\nya.ru\nmaps.me\nm.ya.ru\ncom\n7\nya.ru\nya.com\n"
                         "m.maps.me\nmoscow.m.ya.ru\nmaps.com\nmaps.ru\nya.ya\n") == (
                             "Bad\nBad\nBad\nBad\nBad\nGood\nGood\n")
    for invalid in ("", "-1", "10001", "0\n1\na..b", "0\n1", "0\n0\nextra",
                    "1\nUPPER\n0", "1\n" + "a" * 51 + "\n0"):
        run("domains", invalid, ok=False)
    assert run("domains", "0\n0\n") == ""
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder) / "documents.txt"
        path.write_text("a a\nb\na b")
        assert run("search", "a b\nmissing\n", (str(path),)) == (
            "a b: {docid: 0, hitcount: 2} {docid: 2, hitcount: 2} {docid: 1, hitcount: 1}\n"
            "missing:\n")
        run("search", args=(str(Path(folder) / "missing.txt"),), ok=False)
    run("search", ok=False)
    print("PASS: belt CLI examples, malformed input and missing-file diagnostics")


if __name__ == "__main__":
    main()
