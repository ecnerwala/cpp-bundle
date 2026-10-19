// inc/optimize.hpp
#pragma GCC optimize("unroll-loops")
#if defined(__x86_64__) || defined(__i386__)
#pragma GCC target("avx2")
#endif
namespace wala {
inline void disable_denormal_floats() {
#if defined(__x86_64__) || defined(__i386__)
	#define CSR_FLUSH_TO_ZERO         (1 << 15)
	unsigned csr = __builtin_ia32_stmxcsr();
	csr |= CSR_FLUSH_TO_ZERO;
	__builtin_ia32_ldmxcsr(csr);
	#undef CSR_FLUSH_TO_ZERO
#endif
}
}
#include <bits/stdc++.h>
// inc/g.hpp
#ifndef G_HPP
#define G_HPP
struct G { std::vector<int> v; };
#endif
// inc/h.hpp
#ifdef USE_MAP
#include <map>
#endif
inline int h(G& g) { std::sort(g.v.begin(), g.v.end()); return 0; }
// main.cpp
int main() { G g; return h(g); }
