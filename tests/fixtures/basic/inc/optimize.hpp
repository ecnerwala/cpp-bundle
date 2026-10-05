#pragma once

#pragma GCC optimize("unroll-loops")
#pragma GCC target("avx2") // Requires AVX2

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
