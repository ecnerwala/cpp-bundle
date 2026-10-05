#ifndef GUARDED_HPP
#define GUARDED_HPP
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#if defined(__x86_64__) || defined(__i386__)
#pragma GCC target("avx2")
#endif
#pragma GCC diagnostic pop
#  pragma   once
inline int guarded() { return 1; }
#endif
