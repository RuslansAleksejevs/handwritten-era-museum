"""Build and test the Red Belt reconstructions, without downloading anything."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build" / "course-red")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    args.build_dir.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c++17", "-O1" if args.sanitize else "-O2", "-Wall", "-Wextra", "-Wpedantic", "-pthread"]
    if args.sanitize:
        flags += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    for source, name in (("main.cpp", "red"), ("tests.cpp", "tests")):
        subprocess.run(compiler + flags + [str(HERE / source), str(HERE.parents[1] / "search" / "search.cpp"),
                                          "-o", str(args.build_dir / name)], check=True, timeout=120)
    subprocess.run([str(args.build_dir / "tests")], check=True, timeout=60)
    examples = json.loads((HERE / "examples.json").read_text())
    for example in examples:
        result = subprocess.run([str(args.build_dir / "red"), example["mode"]], input=example["input"],
                                text=True, capture_output=True, timeout=15, check=True)
        if result.stdout.split() != example["expected"].split():
            raise AssertionError((example["source_path"], result.stdout, example["expected"]))
    # A large live booking window exercises repeated clients and integer room sums.
    data = "100002\n" + "BOOK 0 hotel 1 1000\n" * 100000 + "CLIENTS hotel\nROOMS hotel\n"
    result = subprocess.run([str(args.build_dir / "red"), "booking"], input=data,
                            text=True, capture_output=True, timeout=15, check=True)
    if result.stdout != "1\n100000000\n":
        raise AssertionError(result.stdout)
    print(f"PASS red CLI: {len(examples)} archived examples and 100000-booking window")


if __name__ == "__main__":
    main()
