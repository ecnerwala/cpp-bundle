# cpp-bundler

`cpp-bundle` inlines a C++ file's `#include`s of your own headers into one file, the way
you would paste them by hand: your code is copied verbatim (comments, macros, pragmas,
formatting), system headers stay as `#include` lines. It is a small
[libTooling](https://clang.llvm.org/docs/LibTooling.html) tool, so include resolution,
`#if` evaluation, include guards and `#pragma once` are all clang's — the tool only
decides which file's text to copy.

```sh
cpp-bundle [--root DIR] [--prelude HDR]... FILE... [-- COMPILER_ARGS...]
```

- Files under `--root` (default: the current directory) are user headers and get inlined
  where they are included; an include that clang skips (guard, `#pragma once`) produces
  nothing. `#pragma once` lines are dropped; include guards are kept as text.
- Nothing else is ever opened — the tool runs with `-nostdinc -nostdinc++`, so the output
  does not depend on the machine's standard library. Any other `#include` is kept as a
  line, once per header name, at the position of its first inclusion.
- `--prelude HDR` emits `#include <HDR>` before everything else. Once `<bits/stdc++.h>`
  has been emitted (prelude or seen in the source), later includes of standard headers
  (`<vector>`, `<cstdio>`, ...) are dropped.
- `COMPILER_ARGS` go to clang unchanged (`-std=c++23 -I src -DLOCAL ...`); conditional
  includes are evaluated with clang's predefined macros plus these.
- Several `FILE`s are bundled into one output, in order, with shared includes deduplicated.

Example:

```sh
cd my-library
cpp-bundle --prelude bits/stdc++.h --prelude cassert \
  verify/some_problem.test.cpp -- -std=c++23 -I src | cpp-minify --check > submission.cpp
```

## Minifying

```sh
cpp-minify [--check] [FILE]
```

Strips comments and all whitespace that is not needed to separate tokens from `FILE`
(default: stdin), keeping one token line per source line and directives on their own
lines. Identifiers are not renamed, so the result works for anything clang can lex,
bundled or not. `--check` re-lexes the output and fails unless it yields exactly the
input's tokens.

## Building

Needs CMake 3.20+ and the Clang/LLVM development packages of one Clang major (20 is what CI
uses; 16+ should work); zlib and zstd are downloaded and built by CMake. On Debian/Ubuntu:

```sh
wget -qO- https://apt.llvm.org/llvm.sh | sudo bash -s -- 20
sudo apt-get install -y libclang-20-dev llvm-20-dev
cmake -S . -B build -DClang_DIR=/usr/lib/llvm-20/lib/cmake/clang
cmake --build build
ctest --test-dir build
```

The binaries link clang/LLVM, libstdc++, zlib and zstd statically; the only runtime dependency is glibc.

### Release binaries

`Dockerfile` builds the same thing reproducibly on Ubuntu 22.04 (glibc 2.35), so the result runs
on any x86_64 Linux with glibc >= 2.35:

```sh
docker build --output type=local,dest=dist .
```

The `release` workflow runs this for every `v*` tag and attaches
`cpp-bundler-<tag>-linux-x86_64.tar.gz` to the GitHub release.

## License

MIT, see [LICENSE](LICENSE).
