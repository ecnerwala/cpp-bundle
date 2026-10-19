#!/bin/sh
# usage: tests/run.sh path/to/cpp-bundle path/to/cpp-minify
# Each tests/fixtures/*/ has main.cpp (+ inc/), expected.cpp, expected.{min,light,full}.cpp and
# optionally args (extra clang options) and files (inputs, default main.cpp).
# Expected outputs are also syntax-checked with $CXX; tests/sysinc stands in for
# <bits/stdc++.h> where the toolchain lacks it.
set -eu
bin=$(realpath "$1")
minify=$(realpath "$2")
sysinc=$(realpath "$(dirname "$0")/sysinc")
cd "$(dirname "$0")/fixtures"
fail=0
for d in */; do
	d=${d%/}
	args=$(cat "$d/args" 2>/dev/null || true)
	files=$(cat "$d/files" 2>/dev/null || echo main.cpp)
	(cd "$d" && "$bin" $args -std=c++23 -I inc $files) | diff "$d/expected.cpp" - || fail=1
	(cd "$d" && "$bin" $args -std=c++23 -I inc $files) | "$minify" --check | diff "$d/expected.min.cpp" - || fail=1
	"$minify" "$d/expected.cpp" | diff "$d/expected.min.cpp" - || fail=1
	for l in light full; do
		"$minify" --level $l --check "$d/expected.cpp" | diff "$d/expected.$l.cpp" - || fail=1
	done
	for f in "$d/expected.cpp" "$d/expected.min.cpp" "$d/expected.light.cpp" "$d/expected.full.cpp"; do
		"${CXX:-c++}" -std=c++23 -fsyntax-only -I "$sysinc" -x c++ "$f" || fail=1
	done
done
exit $fail
