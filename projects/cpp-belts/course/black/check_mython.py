"""Compile and run the standalone Mython and direct lexer/runtime checks."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    flags = ["-std=c++17", "-O1" if args.sanitize else "-O2", "-Wall", "-Wextra", "-Wpedantic"]
    if args.sanitize:
        flags += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    with tempfile.TemporaryDirectory(prefix="museum-mython-") as temporary:
        binary = Path(temporary) / "mython"
        subprocess.run(compiler+flags+[str(HERE/"mython.cpp"), "-o", str(binary)], check=True, timeout=90)
        command = [sys.executable, str(HERE/"tests_mython.py"), str(binary), "--library-check", "--cxx", compiler[-1]]
        if args.sanitize:
            command += ["--sanitize"]
        subprocess.run(command, check=True, timeout=90)


if __name__ == "__main__":
    main()
