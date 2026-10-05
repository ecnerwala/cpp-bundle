#ifndef GUARDED_HPP
#define GUARDED_HPP
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC target("avx2")
#pragma GCC diagnostic pop
#  pragma   once
inline int guarded() { return 1; }
#endif
