#include <cassert>
#line 1 "inc/engine.hpp"

#line 1 "inc/base.hpp"

#include <vector>
struct Base { std::vector<int> v; };
#include <utility>
#line 4 "inc/engine.hpp"
inline Base engine(Base b) { return std::move(b); }
#line 1 "inc/series.hpp"

#include <algorithm>
inline void series(Base& b) { assert(!b.v.empty()); std::sort(b.v.begin(), b.v.end()); }
#line 4 "main.cpp"
int main() { Base b = engine(Base{{1}}); series(b); return 0; }
