# cpp-bundle

`cpp-bundle` inlines a C++ file's `#include`s of your own headers into one file, the way
you would paste them by hand: your code is copied verbatim (comments, macros, pragmas,
formatting), system headers stay as `#include` lines. It is a small
[libTooling](https://clang.llvm.org/docs/LibTooling.html) tool, so include resolution,
`#if` evaluation, include guards and `#pragma once` are all clang's — the tool only
decides which file's text to copy.

```sh
cpp-bundle [CLANG_ARGS...] FILE...
```

- `#include "..."` is a user header and gets inlined where it is included (an error if the
  file does not exist); an include that clang skips (guard, `#pragma once`) produces nothing.
  Each inlined region is preceded by a `#line N "path"` marker (paths relative to the current
  directory), so compiler diagnostics point at the original files. `#pragma once` lines are
  blanked; include guards are kept as text.
- `#include <...>` is kept as a line, once per header name, at the position of its first
  inclusion. The standard include directories are never searched (the tool runs with
  `-nostdinc -nostdinc++`), so the output does not depend on the machine's standard library.
- `CLANG_ARGS` are clang's own (`-std=c++23 -I src -DLOCAL ...`); conditional includes are
  evaluated with clang's predefined macros plus these. `-include HDR` puts `HDR` first:
  inlined if it is a file, emitted as `#include <HDR>` otherwise. Once `<bits/stdc++.h>` has
  been emitted (`-include` or seen in the source), later includes of standard headers
  (`<vector>`, `<cstdio>`, ...) are dropped.
- Several `FILE`s are bundled into one output, in order, with shared includes deduplicated.

Example:

```sh
cd my-library
cpp-bundle -include bits/stdc++.h -include cassert -std=c++23 -I src \
  verify/some_problem.test.cpp | cpp-minify --check > submission.cpp
```

## Minifying

```sh
cpp-minify [--level light|medium|full] [--width N] [--check] [FILE]
```

Strips comments and whitespace from `FILE` (default: stdin). `light` drops only comments,
blank lines and trailing whitespace; `medium` (default) also drops indentation and every
space not needed to separate tokens, keeping one line per source line; `full` additionally
packs consecutive lines onto lines of up to `--width` (120) characters. Directives always
keep their own line (with single spaces). `#line` markers (as emitted by `cpp-bundle`) are replaced by one `// path` comment
per run of lines from the same file, so the output is safe to paste anywhere and still
says where each part came from. Identifiers are not renamed, so the result works for
anything clang can lex, bundled or not. `--check` re-lexes the output and fails unless it
yields exactly the input's tokens (`#line` directives excepted).

## Installing

Every GitHub release has a tarball of the two binaries per platform (Linux x86_64/aarch64,
macOS arm64) and the same binaries as Python wheels, so projects that already use
`uv`/`pip` get them on `PATH` with no extra tooling:

```sh
uv tool install "cpp-bundle @ https://github.com/ecnerwala/cpp-bundle/releases/download/v0.1.0/cpp_bundle-0.1.0-py3-none-manylinux_2_34_x86_64.whl"
# or, inside a uv project (pins it in uv.lock):
uv add "cpp-bundle @ https://github.com/ecnerwala/cpp-bundle/releases/download/v0.1.0/cpp_bundle-0.1.0-py3-none-manylinux_2_34_x86_64.whl"
```

The wheels contain no Python code; `pyproject.toml` / `hatch_build.py` just wrap `dist/`
(`CPP_BUNDLE_VERSION=x.y.z uv build --wheel` after a build), and on Linux `auditwheel repair`
tags them with the glibc floor the binaries actually need.

## Building

Needs CMake 3.20+ and the Clang/LLVM development packages of one Clang major (23 is what CI
uses); zlib and zstd are downloaded and built by CMake. On Debian/Ubuntu
(macOS: `brew install llvm` and `-DClang_DIR="$(brew --prefix llvm)/lib/cmake/clang"`):

```sh
wget -qO- https://apt.llvm.org/llvm.sh | sudo bash -s -- 23
sudo apt-get install -y clang-23 libclang-23-dev llvm-23-dev
cmake -S . -B build -DClang_DIR=/usr/lib/llvm-23/lib/cmake/clang
cmake --build build
ctest --test-dir build
```

The binaries link clang/LLVM, libstdc++, zlib and zstd statically; the only runtime dependency is
libc (glibc on Linux, the system libc++/libSystem on macOS).

### Release binaries

`Dockerfile` builds the same thing reproducibly on Ubuntu 22.04 (glibc 2.35), so the result runs
on any Linux (x86_64 or aarch64, matching the Docker host) with glibc >= 2.35:

```sh
docker build --output type=local,dest=dist .
```

The `release` workflow runs this on x86_64 and arm64 runners (and a native Homebrew-LLVM build
on macOS arm64) for every `v*` tag and attaches `cpp-bundle-<tag>-<platform>.tar.gz`
and the wheels to the GitHub release.

## License

MIT, see [LICENSE](LICENSE).
