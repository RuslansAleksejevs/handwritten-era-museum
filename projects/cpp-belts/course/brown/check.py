"""Build and test Brown Belt reconstructions; original grader acceptance is not claimed."""
import argparse
import json
import math
import os
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def close(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and actual.keys() == expected.keys() and all(
            close(actual[key], value) for key, value in expected.items())
    if isinstance(expected, list):
        return isinstance(actual, list) and len(actual) == len(expected) and all(
            close(a, b) for a, b in zip(actual, expected))
    if isinstance(expected, (float, int)) and not isinstance(expected, bool):
        return isinstance(actual, (float, int)) and math.isclose(actual, expected, rel_tol=1e-5, abs_tol=1e-7)
    return actual == expected


def text_close(actual, expected):
    left, right = actual.split(), expected.split()
    if len(left) != len(right):
        return False
    for a, b in zip(left, right):
        if a == b:
            continue
        try:
            if not math.isclose(float(a), float(b), rel_tol=1e-5, abs_tol=1e-7):
                return False
        except ValueError:
            return False
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build" / "course-brown")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    args.build_dir.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c++17", "-O1" if args.sanitize else "-O2", "-Wall", "-Wextra", "-Wpedantic", "-pthread", "-fno-rtti"]
    if args.sanitize:
        flags += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    for source, name in (("main.cpp", "brown"), ("tests.cpp", "tests")):
        subprocess.run(compiler + flags + [str(HERE / source), "-o", str(args.build_dir / name)],
                       check=True, timeout=120)
    subprocess.run([str(args.build_dir / "tests")], check=True, timeout=60)
    examples = json.loads((HERE / "examples.json").read_text())
    for example in examples:
        result = subprocess.run([str(args.build_dir / "brown"), example["mode"]], input=example["input"],
                                text=True, capture_output=True, timeout=15, check=True)
        matches = close(json.loads(result.stdout), json.loads(example["expected"])) if example["format"] == "json" else text_close(result.stdout, example["expected"])
        if not matches:
            raise AssertionError((example["source_path"], result.stdout, example["expected"]))
    domains = "3\nya.ru\nm.ya.ru\ncom\n5\nya.ru\nx.ya.ru\nnotya.ru\nru\na.com\n"
    result = subprocess.run([str(args.build_dir / "brown"), "domains"], input=domains,
                            text=True, capture_output=True, timeout=15, check=True)
    if result.stdout != "Bad\nBad\nGood\nGood\nBad\n":
        raise AssertionError("domain reading/output regression: " + result.stdout)
    # Professional-mobile workload: 100000 commands, with an analytic total.
    count = 99999
    data = f"{count + 1}\n" + "Earn 2000-01-01 2099-12-31 36525\n" * count + "ComputeIncome 2000-01-01 2099-12-31\n"
    result = subprocess.run([str(args.build_dir / "brown"), "budget"], input=data,
                            text=True, capture_output=True, timeout=15, check=True)
    if not math.isclose(float(result.stdout), count * 36525, rel_tol=1e-12):
        raise AssertionError("large budget workload total")
    print(f"PASS brown CLI: {len(examples)} archived examples, domain boundaries and input/output, 100000 budget commands")


if __name__ == "__main__":
    main()
