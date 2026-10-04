#!/bin/sh
# usage: tests/run.sh path/to/cpp-bundle path/to/cpp-minify
# Each tests/fixtures/*/ has main.cpp (+ inc/), expected.cpp, expected.min.cpp and
# optionally args (extra cpp-bundle options).
set -eu
bin=$(realpath "$1")
minify=$(realpath "$2")
cd "$(dirname "$0")/fixtures"
fail=0
for d in */; do
	d=${d%/}
	args=$(cat "$d/args" 2>/dev/null || true)
	(cd "$d" && "$bin" $args main.cpp -- -std=c++23 -I inc) | diff "$d/expected.cpp" - || fail=1
	(cd "$d" && "$bin" $args --minify main.cpp -- -std=c++23 -I inc) | diff "$d/expected.min.cpp" - || fail=1
	"$minify" --check "$d/expected.cpp" | diff "$d/expected.min.cpp" - || fail=1
	"$minify" < "$d/expected.cpp" | diff "$d/expected.min.cpp" - || fail=1
	for f in "$d/expected.cpp" "$d/expected.min.cpp"; do
		"${CXX:-c++}" -std=c++23 -fsyntax-only -x c++ "$f" || fail=1
	done
done
exit $fail
