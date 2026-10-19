#line 1 "inc/optimize.hpp"


#pragma GCC optimize("unroll-loops")
#if defined(__x86_64__) || defined(__i386__)
#pragma GCC target("avx2") // Requires AVX2
#endif

// See https://codeforces.com/blog/entry/96344

namespace wala {

inline void disable_denormal_floats() {
	// https://stackoverflow.com/a/8217313
#if defined(__x86_64__) || defined(__i386__)
	#define CSR_FLUSH_TO_ZERO         (1 << 15)
	unsigned csr = __builtin_ia32_stmxcsr();
	csr |= CSR_FLUSH_TO_ZERO;
	__builtin_ia32_ldmxcsr(csr);
	#undef CSR_FLUSH_TO_ZERO
#endif
}

} // namespace wala
#include <bits/stdc++.h>
#line 1 "inc/g.hpp"
#ifndef G_HPP
#define G_HPP
// #include "nothing.hpp"
#line 5 "inc/g.hpp"
/* #include <map>
*/
struct G { std::vector<int> v; };
#endif // G_HPP
#line 1 "inc/h.hpp"

#line 4 "inc/h.hpp"
#ifdef USE_MAP
#include <map>
#endif
inline int h(G& g) { std::sort(g.v.begin(), g.v.end()); return 0; }
#line 7 "main.cpp"
int main() { G g; return h(g); }
