
#pragma GCC optimize("unroll-loops")
#pragma GCC target("avx2") // Requires AVX2

// See https://codeforces.com/blog/entry/96344

namespace wala {

inline void disable_denormal_floats() {
	// https://stackoverflow.com/a/8217313
	#define CSR_FLUSH_TO_ZERO         (1 << 15)
	unsigned csr = __builtin_ia32_stmxcsr();
	csr |= CSR_FLUSH_TO_ZERO;
	__builtin_ia32_ldmxcsr(csr);
	#undef CSR_FLUSH_TO_ZERO
}

} // namespace wala
#include <bits/stdc++.h>
#ifndef G_HPP
#define G_HPP
// #include "nothing.hpp"
#include <vector>
/* #include <map>
*/
struct G { std::vector<int> v; };
#endif // G_HPP
#include <algorithm> // sort
#ifdef USE_MAP
#include <map>
#endif
inline int h(G& g) { std::sort(g.v.begin(), g.v.end()); return 0; }
int main() { G g; return h(g); }
