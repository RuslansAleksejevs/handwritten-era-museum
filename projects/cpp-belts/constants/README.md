[← C++ belts](../README.md) · [The talk](https://www.youtube.com/watch?v=GPAGiXNVED4)

# One constant, two source files

Matrosov's talk made declaring a constant memorable. This small **2026 reconstruction** isolates one question from that topic: if a header defines a constant, do two source files refer to the same object?

| Header declaration in this example | Value | Object identity across files |
| :-- | :-- | :-- |
| `const int local_answer = 42;` | A compile-time constant | A separate object in each translation unit |
| `inline constexpr int shared_answer = 42;` | A compile-time constant | One shared object |

At namespace scope, the first declaration has internal linkage. The C++17 inline variable has external linkage and can be defined identically in multiple translation units. Taking addresses makes the distinction observable. The test calls functions compiled in a second source file and checks the addresses, so equal values alone cannot make it pass.

A string adds a lifetime question. Here `inline constexpr char title[]` owns static storage and `std::string_view` points into it. The view itself does not own characters; copying a view never extends the lifetime of a temporary string.

The language rules are documented in the C++ working draft: [linkage](https://eel.is/c++draft/basic.link), [inline variables](https://eel.is/c++draft/dcl.inline), and [string views](https://eel.is/c++draft/string.view.template).

## Run

From the repository root:

```sh
make constants-check
```

The build passes `main.cpp` and `other.cpp` as separate translation units. Compile-time assertions check constant expressions; explicit runtime checks remain active even if assertions are disabled with `NDEBUG`.

This is a compact study inspired by the talk, not one of the original belt submissions or a substitute for the full talk.
