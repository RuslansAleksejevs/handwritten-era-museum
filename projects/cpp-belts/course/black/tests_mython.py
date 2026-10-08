#!/usr/bin/env python3
"""Standalone Mython CLI tests and independent bounded arithmetic/GCD oracles."""
import argparse
import math
import random
import subprocess
import textwrap
import tempfile
from pathlib import Path

if not __debug__:
    raise SystemExit('assert-based checks need Python without -O or PYTHONOPTIMIZE')

GCD = '''\
class GCD:
  def calc(a, b):
    if a == 0 or b == 0:
      return a + b
    else:
      if a < b:
        return self.calc(b, a)
      else:
        return self.calc(b, a - b)
  def is_coprime(a, b):
    return self.calc(a, b) == 1
'''


def run(binary, program, expected=None, error=None):
    result = subprocess.run([str(binary)], input=textwrap.dedent(program), text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
    if error is not None:
        assert result.returncode != 0, (program, result.stdout)
        assert error in result.stderr, (error, result.stderr)
    else:
        assert result.returncode == 0, (program, result.stderr)
        assert result.stdout == expected, (program, repr(expected), repr(result.stdout))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary')
    parser.add_argument('--library-check', action='store_true')
    parser.add_argument('--sanitize', action='store_true', help='instrument optional C++ API fixture with UBSan')
    parser.add_argument('--cxx', default='clang++')
    options = parser.parse_args()
    binary = options.binary
    checks = 0
    def good(program, expected):
        nonlocal checks
        run(binary, program, expected=expected); checks += 1
    def bad(program, message):
        nonlocal checks
        run(binary, program, error=message); checks += 1

    # Published problem example, with its original implicit-self method signatures.
    good(GCD + '''
        coprime = False
        gcd = None
        gcd = GCD()
        x = 4
        y = 13
        coprime = gcd.is_coprime(x, y)
        if coprime:
          print x, 'and', y, 'are coprime'
        else:
          print x, 'and', y, 'are not coprime'
    '''.replace('        ', ''), '4 and 13 are coprime\n')
    good('', '')
    good(' # comment only\n   \n# eof', '')
    good('print\nprint 1, True, False, None\n', '\n1 True False None\n')
    good('print 2 + 3 * 4, (2 + 3) * 4, -7 / 3, 7 / -3', '14 20 -2 -2\n')
    good('print "a\\nb\\t\\\"c\\\\d", \'it\\\'s\' # comment\n', 'a\nb\t"c\\d it\'s\n')
    good('if True:\r\n  # blank block line\r\n  print "a#b"\r\n\r\nprint "ok"\r\n', 'a#b\nok\n')
    good('print not 0, not "", not None, 3 and 7, 0 or "x"\n', 'True True True True True\n')
    good('print True or missing, False and (1 / 0), not False and True\n', 'True False True\n')
    good('print 1 == 1, 1 != 2, 1 <= 2, 2 >= 2, "a" < "b", False < True, None == None\n', 'True True True True True True True\n')
    good('''
        class Box:
          def __init__(value):
            self.value = value
          def __str__():
            return 'Box(' + str(self.value) + ')'
          def __add__(other):
            return Box(self.value + other.value)
          def __eq__(other):
            return self.value == other.value
          def __lt__(other):
            return self.value < other.value
          def identity():
            return self
        a = Box(2)
        b = Box(3)
        c = a + b
        print c, a.value, b.value
        print a < b, b > a, a <= a, a >= a, a != b, a == Box(2)
        a = a.identity()
        print a.value
    ''', 'Box(5) 2 3\nTrue True True True True True\n2\n')
    good('''
        class Parent:
          def __init__(x):
            self.x = x
          def value():
            return self.x
          def label():
            return 'parent'
        class Child(Parent):
          def label():
            return 'child'
        c = Child(7)
        print c.value(), c.label()
        c.x = 9
        print c.value()
    ''', '7 child\n9\n')
    good('''
        class Factory:
          def __init__(x):
            self.x = x
          def fresh(x):
            item = Factory(x)
            return item
          def link(other):
            self.other = other
          def choose(x):
            if x:
              if True:
                return 'early'
            else:
              return None
            print 'unreachable'
          def empty():
            self.x = self.x + 1
        factory = Factory(0)
        a = factory.fresh(10)
        b = factory.fresh(20)
        alias = a
        a = None
        b.link(alias)
        alias.link(b)
        factory = None
        print alias.x, b.x, b.other.x, alias.other.x
        print alias.choose(True), alias.choose(False), alias.empty()
        print alias.x, b.x
    ''', '10 20 10 20\nearly None None\n11 20\n')
    good('''
        class Counter:
          def __init__():
            self.n = 0
          def next():
            self.n = self.n + 1
            return self.n
          def pair(a, b):
            return a * 10 + b
        c = Counter()
        print c.pair(c.next(), c.next())
        print True or c.next(), False and c.next(), c.n
        print c.next() + c.next()
        print c.n
    ''', '12\nTrue False 2\n7\n4\n')
    good('''
        class Empty:
          def value():
            return None
        a = Empty()
        if a:
          print str(a)
        print a.value() == None
    ''', '<Empty instance>\nTrue\n')
    good('''
        class Local:
          def change(x):
            x = x + 1
            return x
        x = 4
        item = Local()
        print item.change(x), x
    ''', '5 4\n')

    good('print None == 1, None != 1, 1 == None, None == None\n', 'False True False True\n')
    bad('print '+('1+'*3000)+'1\n', 'depth limit')
    bad('print '+('('*300)+'1'+(')'*300)+'\n', 'depth limit')
    bad("x = 'a'\n"+'x = x + x\n'*21, 'string size limit')
    bad('#'+('x'*(1024*1024)), 'source size limit')

    # Faults must report errors rather than crash, return a stale value, or overflow.
    for program, message in [
        ('print 1 / 0\n', 'division by zero'),
        ('print 9223372036854775807 + 1\n', 'overflow'),
        ('print 3037000500 * 3037000500\n', 'overflow'),
        ('print (-9223372036854775807 - 1) / -1\n', 'overflow'),
        ('print -(-9223372036854775807 - 1)\n', 'overflow'),
        ('print 9223372036854775808\n', 'literal overflow'),
        ('print "x" + 1\n', 'string addition'),
        ('print True + 1\n', 'arithmetic requires'),
        ('print absent\n', 'unknown variable'),
        ('print 1.missing\n', 'expected class instance'),
        ('return 1\n', 'return outside'),
        ('if True:\n print 1\n', 'pairs of spaces'),
        ('if True:\n\tprint 1\n', 'tabs'),
        ('if True:\n    print 1\n', 'expected expression'),
        ('print "unfinished\n', 'unterminated string'),
        ('print "bad\\q"\n', 'unknown string escape'),
        ('print 12abc\n', 'integer suffix'),
        ('print 1 < 2 < 3\n', 'unexpected token'),
        ('class A(B):\n  def f():\n    return 1\n', 'base class'),
        ('class A:\n  def f(self):\n    return 1\n', 'self is implicit'),
        ('class A:\n  def f(x, x):\n    return x\n', 'parameter names'),
        ('class A:\n  def f():\n    return 1\n  def f():\n    return 2\n', 'duplicate method'),
        ('class A:\n  def f():\n    return 1\na = A(3)\n', 'takes no arguments'),
        ('class A:\n  def f(x):\n    return x\na = A()\nprint a.f()\n', 'wrong argument count'),
        ('class A:\n  def f():\n    return 1\na = A()\nprint a.x\n', 'unknown field'),
        ('class A:\n  def f():\n    return 1\na = A()\na.nope()\n', 'method not found'),
        ('class A:\n  def __str__():\n    return 1\nprint A()\n', '__str__ must'),
        ('class A:\n  def __eq__(x):\n    return 1\nprint A() == A()\n', 'must return bool'),
        ('class A:\n  def forever():\n    return self.forever()\na = A()\na.forever()\n', 'depth limit'),
    ]:
        bad(program, message)

    # Oracles are computed in Python, independent of the interpreter's AST/runtime.
    rng = random.Random(507)
    programs, answers = [], []
    for _ in range(250):
        a, b, c = [rng.randrange(-100, 101) for _ in range(3)]
        d = rng.choice([i for i in range(-20, 21) if i])
        programs.append(f'print ({a} + {b}) * {c}, {a} / {d}, {a} < {b}, {a} == {b}')
        quotient = (abs(a) // abs(d)) * (-1 if (a < 0) != (d < 0) else 1)
        answers.append(f'{(a+b)*c} {quotient} {a < b} {a == b}')
    good('\n'.join(programs), '\n'.join(answers)+'\n')
    # The course's subtraction GCD recurses about twice per step: (1, 120) nests
    # roughly 240 method calls, which the old 256-frame runtime limit rejected.
    pairs = [(0,0),(0,13),(13,0),(4,13),(12,18),(21,14),(17,19),(24,16),(9,27),(100,3),(1,120)]
    for a, b in pairs:
        good(GCD+f'g = GCD()\nprint g.calc({a}, {b})\n', str(math.gcd(a,b))+'\n')
    if options.library_check:
        library_check(options)
    print(f'PASS: {checks} Mython programs, including 250 independent arithmetic cases '
          f'and {len(pairs) + 1} GCD cases')

def library_check(options):
    source = r"""
#include "mython.hpp"
#include <sstream>
#include <iostream>
#include <thread>
using namespace museum::mython;
void Check(bool result) { if (!result) throw std::runtime_error("API assertion failed"); }
int main() {
    std::istringstream source("if True:\n  print 'a\\n', 42\n\n  # ignored\n  print False\nprint None");
    Lexer lexer(source);
    Check(lexer.CurrentToken().kind == Kind::If);
    lexer.ExpectNext(Kind::True);
    lexer.ExpectNext(Kind::Symbol, ":");
    lexer.ExpectNext(Kind::Newline);
    lexer.ExpectNext(Kind::Indent);
    lexer.ExpectNext(Kind::Print);
    Check(lexer.ExpectNext(Kind::String).text == "a\n");
    lexer.ExpectNext(Kind::Symbol, ",");
    Check(lexer.ExpectNext(Kind::Number).number == 42);
    lexer.ExpectNext(Kind::Newline);
    lexer.ExpectNext(Kind::Print);
    lexer.ExpectNext(Kind::False);
    lexer.ExpectNext(Kind::Newline);
    lexer.ExpectNext(Kind::Dedent);
    lexer.ExpectNext(Kind::Print);
    lexer.ExpectNext(Kind::None);
    lexer.ExpectNext(Kind::Newline);
    lexer.ExpectNext(Kind::Eof);
    lexer.ExpectNext(Kind::Eof);
    bool failed = false;
    try { lexer.Expect(Kind::Number); } catch (const Error&) { failed = true; }
    Check(failed);
    failed = false;
    std::istringstream symbol("print 1 + 2");
    Lexer other(symbol);
    other.NextToken(); other.NextToken();
    try { other.Expect(Kind::Symbol, "-"); } catch (const Error&) { failed = true; }
    Check(failed);
    // Fresh independent Runtime arena for every call, including cyclic fields.
    for (int i=0; i<100; ++i) {
        std::istringstream program("class A:\n  def __init__():\n    self.next = self\n  def f():\n    return self.next\na = A()\nprint a.f().next.f()\n");
        std::ostringstream out;
        Run(program,out);
        Check(out.str() == "<A instance>\n");
    }
    std::istringstream isolated("print a\n");
    std::ostringstream out;
    failed = false;
    try { Run(isolated,out); } catch (const Error&) { failed = true; }
    Check(failed);
    // Preserve the two-argument API's bounded recursion in a worker thread.
    // On macOS its default stack is much smaller than the main thread's.
    bool depth_error = false;
    std::thread worker([&] {
        std::istringstream recursive("class A:\n  def f():\n    return self.f()\na = A()\nprint a.f()\n");
        std::ostringstream sink;
        try { Run(recursive, sink); }
        catch (const Error& error) {
            depth_error = std::string(error.what()).find("depth limit") != std::string::npos;
        }
    });
    worker.join();
    Check(depth_error);
    const std::string countdown = "class A:\n  def f(n):\n    if n == 0:\n      return 0\n    return self.f(n - 1)\na = A()\nprint a.f(120)\n";
    std::istringstream conservative(countdown), expanded(countdown);
    std::ostringstream sink;
    failed = false;
    try { Run(conservative, sink); } catch (const Error&) { failed = true; }
    Check(failed);
    Run(expanded, sink, 2048);
    Check(sink.str() == "0\n");
    for (const std::size_t limit : {0u, 2049u}) {
        std::istringstream empty;
        failed = false;
        try { Run(empty, sink, limit); } catch (const Error&) { failed = true; }
        Check(failed);
    }
    std::istringstream minimal;
    Run(minimal, sink, 1);
    std::cout << "PASS: lexer API, 100 isolated cyclic-object runs and default/explicit runtime limits\n";
}
"""
    folder = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix='mython-api-') as temporary:
        fixture = Path(temporary)/'check.cpp';fixture.write_text(source)
        binary = Path(temporary)/'check'
        # Match the CLI sanitizer build; debug builds need a larger native stack.
        flags = ['-O1', '-fsanitize=undefined', '-fno-sanitize-recover=all'] if options.sanitize else ['-O2']
        subprocess.run([options.cxx, '-std=c++17', '-pthread', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                        '-DMYTHON_LIBRARY', *flags, '-I', str(folder), str(fixture), str(folder/'mython.cpp'),
                        '-o', str(binary)], check=True, timeout=60)
        subprocess.run([str(binary)], check=True, timeout=10)

if __name__ == '__main__': main()
