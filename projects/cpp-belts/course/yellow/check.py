"""Build isolated C++17 programs and run local contract/oracle checks."""
import argparse
import os
import shlex
import subprocess
import tempfile
from pathlib import Path
from test_cli import check
if not __debug__:raise SystemExit('assert-based checks need Python without -O or PYTHONOPTIMIZE')
ROOT=Path(__file__).resolve().parent
SOURCES=['sum_reverse_sort.cpp','phone_number.cpp','rectangle.cpp','buses/query.cpp','buses/responses.cpp','buses/bus_manager.cpp','events/date.cpp','events/node.cpp','events/token.cpp','events/condition_parser.cpp','events/database.cpp']

def main():
 p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--build',type=Path);args=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='museum-yellow-') as temporary:
  build=args.build.resolve() if args.build else Path(temporary);build.mkdir(parents=True,exist_ok=True)
  compiler=shlex.split(os.environ.get('CXX','c++'))
  flags=['-std=c++17','-O1' if args.sanitize else '-O2','-Wall','-Wextra','-Wpedantic']
  if args.sanitize:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
  objects=[]
  for i,source in enumerate(SOURCES):
   obj=build/f'part-{i}.o';subprocess.run(compiler+flags+['-c',str(ROOT/source),'-o',str(obj)],check=True,timeout=60);objects.append(str(obj))
  for source,name in [('main.cpp','yellow'),('tests.cpp','test-yellow'),('buses/main.cpp','yellow-buses'),('events/main.cpp','yellow-events')]:
   subprocess.run(compiler+flags+[str(ROOT/source),*objects,'-o',str(build/name)],check=True,timeout=90)
  subprocess.run([str(build/'test-yellow')],check=True,timeout=30)
  check(build/'yellow')
  # Both multi-file course executables are linked and actually exercised.
  for name,source,expected in [('yellow-buses','1\nALL_BUSES\n','No buses\n'),('yellow-events','Find\n','Found 0 entries\n')]:
   r=subprocess.run([str(build/name)],input=source,text=True,capture_output=True,check=True,timeout=10);assert r.stdout==expected

if __name__=='__main__':main()
