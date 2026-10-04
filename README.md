# cpp-bundler

`cpp-bundle` inlines a C++ file's `#include`s of your own headers into one file, the way
you would paste them by hand: your code is copied verbatim (comments, macros, pragmas,
formatting), system headers stay as `#include` lines. It is a small
[libTooling](https://clang.llvm.org/docs/LibTooling.html) tool, so include resolution,
`#if` evaluation, include guards and `#pragma once` are all clang's — the tool only
decides which file's text to copy.

```sh
cpp-bundle [--minify] [--root DIR] FILE [-- COMPILER_ARGS...]
```

- Files under `--root` (default: the current directory) are user headers and get inlined
  where they are included; an include that clang skips (guard, `#pragma once`) produces
  nothing. `#pragma once` lines are dropped; include guards are kept as text.
- Any other include is a system header: its body is skipped and the `#include` line is
  emitted once, at the position of its first inclusion.
- `COMPILER_ARGS` go to clang unchanged (`-std=c++23 -I src -DLOCAL ...`); conditional
  includes are evaluated with clang's predefined macros plus these.
- `--minify` strips comments and all whitespace that is not needed to separate tokens,
  keeping one token line per source line and directives on their own lines. Identifiers
  are not renamed.

Example:

```sh
cd my-library
cpp-bundle --minify verify/some_problem.test.cpp -- -std=c++23 -I src > submission.cpp
```

## Building

Needs CMake and the Clang/LLVM development packages of one Clang major (20 is what CI
uses; 16+ should work). On Debian/Ubuntu:

```sh
wget -qO- https://apt.llvm.org/llvm.sh | sudo bash -s -- 20
sudo apt-get install -y libclang-20-dev llvm-20-dev
cmake -S . -B build -DClang_DIR=/usr/lib/llvm-20/lib/cmake/clang
cmake --build build
ctest --test-dir build
```

The binary links `libclang-cpp` and `libLLVM` dynamically and bakes in that Clang's
resource directory (its own `stddef.h`, `immintrin.h`, ...); pass `-resource-dir` after
`--` to override it. C++ standard headers come from the GCC installation clang finds on
the system, as with `clang++` itself.

## License

MIT, see [LICENSE](LICENSE).
