#!/bin/sh
# usage: tests/run.sh path/to/cpp-bundle
# Each tests/fixtures/*/ has main.cpp (+ inc/), expected.cpp and expected.min.cpp.
set -eu
bin=$(realpath "$1")
cd "$(dirname "$0")/fixtures"
fail=0
for d in */; do
	d=${d%/}
	(cd "$d" && "$bin" main.cpp -- -std=c++23 -I inc) | diff "$d/expected.cpp" - || fail=1
	(cd "$d" && "$bin" --minify main.cpp -- -std=c++23 -I inc) | diff "$d/expected.min.cpp" - || fail=1
	for f in "$d/expected.cpp" "$d/expected.min.cpp"; do
		"${CXX:-c++}" -std=c++23 -fsyntax-only -x c++ "$f" || fail=1
	done
done
exit $fail
