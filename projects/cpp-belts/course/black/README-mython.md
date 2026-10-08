[← C++ exhibits](../../README.md)

# Mython: from characters to objects

An independent **2026 reconstruction** of the Black Belt lexer/interpreter exercises. The historical course supplied a parser; this exhibit also implements its own recursive-descent parser. This is modern Codex-written code under the collection owner's supervision, not a recovered original solution or a claim of historical course completion.

[Lexer and runtime API](mython.hpp) · [Implementation and CLI](mython.cpp) · [Tests](tests_mython.py)

## Run a program

From the repository root:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
  projects/cpp-belts/course/black/mython.cpp -o /tmp/mython
/tmp/mython < program.my
python3 projects/cpp-belts/course/black/tests_mython.py /tmp/mython --library-check
```

The executable reads a whole program from standard input, parses it before execution, and writes `print` output to standard output. Parse/runtime errors return a nonzero status with a line number on standard error. Output produced before a runtime error remains visible.

```python
class Box:
  def __init__(value):
    self.value = value
  def __str__():
    return 'Box(' + str(self.value) + ')'
  def __add__(other):
    return Box(self.value + other.value)

print Box(2) + Box(3)
```

Output: `Box(5)`.

**`self` is implicit in Mython method signatures**: write `def value(x):`, then refer to `self` in the body. Do not add Python's explicit `self` parameter.

## Language and implementation

- The lexer recognizes names, decimal integers, quoted strings, keywords, symbols, comparisons, newline/indent/dedent and EOF. Indentation uses **two spaces per level**; tabs are rejected. Blank lines and `#` comments do not change indentation. Both quote styles support escaped quotes, backslash, newline, tab and carriage return. CRLF and a missing final newline are accepted.
- The parser implements parentheses, unary signs, `* /`, `+ -`, one comparison, `not`, `and`, then `or` in precedence order. It handles variable/field assignment, `print`, method calls, `return`, `if/else`, classes and single inheritance. Free functions, loops, containers and chained comparisons are outside this language.
- Values are **checked signed 64-bit integers**, strings, booleans, `None`, classes and instances. Integer division truncates toward zero: `-7 / 3` is `-2`. Integer overflow and division by zero produce errors. String addition concatenates; instance addition calls `__add__`.
- `and`/`or` short-circuit and return **booleans**, as in the course's boolean operators. Zero, an empty string, `False` and `None` are false; instances are true. This is not Python's operand-returning `and`/`or` convention.
- Methods have local parameters and implicit `self`; instance state belongs to fields. Arguments evaluate once, left to right. A missing return gives `None`; an executed return leaves nested blocks immediately. Inherited methods and initializers are found through the parent chain; child methods override them.
- Printing and `str` use `__str__`, which must return a string. The deterministic fallback is `<ClassName instance>`. `__eq__` and `__lt__` must return booleans; the remaining comparisons derive from those operations. `None` equals only `None`; incompatible primitive comparisons fail instead of silently converting types.

This exposes a small `museum::mython::Lexer` with `CurrentToken`, `NextToken` and checked `Expect`/`ExpectNext` operations, plus `Run(istream, ostream)`. Compile with `-DMYTHON_LIBRARY` when linking it into another C++ program. The namespace/API are this reconstruction's interface, not a promise of drop-in compatibility with the historical grader headers.

## Object lifetime is part of the interpreter

Each `Run` owns an arena of class and instance objects. Values carry non-owning pointers to stable arena objects. Returning an instance, overwriting a variable, constructing a second instance at the same call site, or creating `a.other = b; b.other = a` cannot destroy a still-referenced object. The whole arena is reclaimed when `Run` finishes, including cycles and error paths. There is no garbage collection during a run, so unreachable objects remain allocated until that run ends.

Guardrails bound source text to 1 MiB, tokens to 20,000, AST nodes to 8,192, parser nesting to 256, executed steps to 1,000,000, instances to 100,000 and strings to 1 MiB. The CLI allows 2,048 interpreter frames, about 250 nested calls of the published GCD method. A method call uses several frames.

The two-argument library API `Run(input, output)` retains its conservative **256-frame** runtime limit. An embedding program can explicitly choose 1–2,048 frames with `Run(input, output, limit)` when it has sufficient native stack. The default was checked with a 512 KiB worker stack in optimized and `-O1` UBSan builds on macOS arm64. An unoptimized UBSan build exhausts that small stack even at 256 frames; debug builds need a larger stack or a lower explicit limit. The CLI was checked on the main thread with its 8 MiB stack. These are local observations, not portable minimum-stack guarantees.

With sufficient native stack, guards turn excessive nesting and integer overflow into language errors. These limits make the exhibit predictable; this is not a security sandbox for hostile native extensions.

## Checks

The suite runs **62 programs**, including the published coprime/GCD example, **250 independently computed arithmetic cases**, eleven additional GCD cases (one nests about 240 method calls), inheritance, method arity, operator methods, side-effect order, short-circuit evaluation, nested return, instance aliases/cycles and invalid-input cases. The optional C++ fixture checks the lexer API and **100 separate `Run` calls with cyclic objects**, then checks that variables do not leak between runs, the default recursion guard in a worker thread, explicit deeper recursion and invalid limit options.

```sh
c++ -std=c++17 -O1 -fsanitize=undefined -fno-sanitize-recover=all \
  projects/cpp-belts/course/black/mython.cpp -o /tmp/mython-ubsan
python3 projects/cpp-belts/course/black/tests_mython.py /tmp/mython-ubsan \
  --library-check --sanitize
```

The normal and UBSan commands were run locally. These are targeted contract/oracle checks; they do not establish compatibility with every hidden course test.

**Exercise provenance:** [lexer statement](https://github.com/ivtsylin/BeltsCpp/tree/87c0b9ba255c8716a559d1d56c87cd06a5701e5e/05-Black-Belt/03-Third-week/02-Lexer) · [interpreter statement and supplied parser interfaces](https://github.com/ivtsylin/BeltsCpp/tree/87c0b9ba255c8716a559d1d56c87cd06a5701e5e/05-Black-Belt/03-Third-week/03-Interpret). The supplied parser was consulted as a grammar reference; its implementation is not vendored here.
