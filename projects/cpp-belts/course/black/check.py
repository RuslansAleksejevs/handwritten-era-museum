"""Build Black Belt cores and run local checks; no downloads or grader claims."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from test_cli import check

if not __debug__:
    raise SystemExit("assert-based checks need Python without -O or PYTHONOPTIMIZE")

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--protobuf", action="store_true", help="also use installed official Python protobuf")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="museum-black-") as temporary:
        build = args.build.resolve() if args.build else Path(temporary)
        build.mkdir(parents=True, exist_ok=True)
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        flags = ["-std=c++17", "-O1" if args.sanitize else "-O2", "-Wall", "-Wextra", "-Wpedantic"]
        if args.sanitize:
            flags += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
        for source, name in [("main.cpp", "black"), ("tests.cpp", "test-black")]:
            subprocess.run(compiler+flags+[str(HERE/source), "-o", str(build/name)], check=True, timeout=90)
        subprocess.run([str(build/"test-black")], check=True, timeout=40)
        check(build/"black", args.protobuf)

        def compiles(chain):
            source = '#include "json_printer.hpp"\n#include <sstream>\nusing namespace museum::black;\nint main(){std::ostringstream o;'+chain+'}\n'
            return subprocess.run(compiler+["-std=c++17", "-x", "c++", "-fsyntax-only", "-I", str(HERE), "-"],
                                  input=source, text=True, capture_output=True, timeout=20).returncode == 0

        # A valid chain must compile, so the rejections test the builder's types, not the harness.
        assert compiles("PrintJsonObject(o).Key(\"x\").BeginArray().Number(1).EndArray().EndObject();")
        for bad in ["PrintJsonArray(o).Key(\"x\");", "PrintJsonObject(o).Number(1);",
                    "PrintJsonObject(o).Key(\"x\").Key(\"y\");", "PrintJsonArray(o).EndArray().Number(1);"]:
            assert not compiles(bad), bad
        print("Black JSON builder: a valid chain compiles; four invalid chains rejected at compile time PASS")


if __name__ == "__main__":
    main()
