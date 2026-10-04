#pragma GCC optimize("unroll-loops")
#pragma GCC target("avx2")
namespace wala{
inline void disable_denormal_floats(){
#define CSR_FLUSH_TO_ZERO (1 << 15)
unsigned csr=__builtin_ia32_stmxcsr();
csr|=CSR_FLUSH_TO_ZERO;
__builtin_ia32_ldmxcsr(csr);
#undef CSR_FLUSH_TO_ZERO
}
}
#include <bits/stdc++.h>
#ifndef G_HPP
#define G_HPP
#include <vector>
struct G{std::vector<int>v;};
#endif
#include <algorithm>
#ifdef USE_MAP
#include <map>
#endif
inline int h(G&g){std::sort(g.v.begin(),g.v.end());return 0;}
int main(){G g;return h(g);}
