#include <cassert>
// inc/base.hpp
#include <vector>
struct Base{std::vector<int>v;};
#include <utility>
// inc/engine.hpp
inline Base engine(Base b){return std::move(b);}
// inc/series.hpp
#include <algorithm>
inline void series(Base&b){assert(!b.v.empty());std::sort(b.v.begin(),b.v.end());}
// main.cpp
int main(){Base b=engine(Base{{1}});series(b);return 0;}
