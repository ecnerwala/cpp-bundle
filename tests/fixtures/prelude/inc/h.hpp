#pragma once
#include "g.hpp"
#include <algorithm> // sort
#ifdef USE_MAP
#include <map>
#endif
inline int h(G& g) { std::sort(g.v.begin(), g.v.end()); return 0; }
