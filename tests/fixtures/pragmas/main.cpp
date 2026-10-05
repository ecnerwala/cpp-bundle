#pragma GCC optimize("O3")
#include "opt.hpp"
#include "guarded.hpp"
#include "opt.hpp"
#include "guarded.hpp"
#include <cstdio>
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma STDC FP_CONTRACT ON
int main() {
	#pragma unroll
	for (int i = 0; i < 2; i++) std::printf("%d\n", opt(i) + guarded());
	warn();
	return 0;
}
