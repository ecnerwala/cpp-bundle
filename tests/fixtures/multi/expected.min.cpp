#include <cassert>
#include <vector>
struct Base{std::vector<int>v;};
#include <utility>
inline Base engine(Base b){return std::move(b);}
#include <algorithm>
inline void series(Base&b){assert(!b.v.empty());std::sort(b.v.begin(),b.v.end());}
int main(){Base b=engine(Base{{1}});series(b);return 0;}
