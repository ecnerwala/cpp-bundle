#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")
#pragma GCC optimize("Ofast")
#pragma GCC target("sse,sse2,sse3,ssse3,sse4,popcnt,abm,bmi,bmi2,mmx,avx,avx2,fma") // Requires AVX2
#pragma clang optimize off
#pragma clang attribute push(__attribute__((noinline)), apply_to = function)
inline int opt(int x) { return x * 2; }
#pragma clang attribute pop
#pragma clang optimize on
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wunused-variable\"")
inline void warn() { int unused = 0; }
_Pragma("GCC diagnostic pop")
#ifndef GUARDED_HPP
#define GUARDED_HPP
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC target("avx2")
#pragma GCC diagnostic pop
inline int guarded() { return 1; }
#endif
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
