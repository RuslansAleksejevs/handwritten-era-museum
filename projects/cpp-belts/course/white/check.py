"""Build and check White Belt locally; no downloads or grader claims."""
import argparse
import os
import shlex
import subprocess
import tempfile
from pathlib import Path
from test_cli import check

if not __debug__:
    raise SystemExit('assert-based checks need Python without -O or PYTHONOPTIMIZE')

ROOT=Path(__file__).resolve().parent

def main():
    p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--build',type=Path);args=p.parse_args()
    with tempfile.TemporaryDirectory(prefix='museum-white-') as temporary:
        build=args.build.resolve() if args.build else Path(temporary)
        build.mkdir(parents=True,exist_ok=True)
        flags=['-std=c++17','-O1' if args.sanitize else '-O2','-Wall','-Wextra','-Wpedantic']
        if args.sanitize: flags += ['-fsanitize=undefined','-fno-sanitize-recover=all']
        for source,name in [('main.cpp','white'),('tests.cpp','test-white')]:
            subprocess.run(shlex.split(os.environ.get('CXX','c++'))+flags+[str(ROOT/source),'-o',str(build/name)],check=True,timeout=90)
        subprocess.run([str(build/'test-white')],check=True,timeout=30)
        check(build/'white')

if __name__=='__main__':main()
