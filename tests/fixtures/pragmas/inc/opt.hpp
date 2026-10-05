#pragma once
#pragma GCC optimize("unroll-loops")
#pragma GCC optimize("Ofast")
#if defined(__x86_64__) || defined(__i386__)
#pragma GCC target("sse,sse2,sse3,ssse3,sse4,popcnt,abm,bmi,bmi2,mmx,avx,avx2,fma") // Requires AVX2
#endif
#pragma clang optimize off
#pragma clang attribute push(__attribute__((noinline)), apply_to = function)
inline int opt(int x) { return x * 2; }
#pragma clang attribute pop
#pragma clang optimize on
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wunused-variable\"")
inline void warn() { int unused = 0; }
_Pragma("GCC diagnostic pop")
