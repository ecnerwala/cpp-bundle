#pragma once
#include "base.hpp"
#include <algorithm>
#include <cassert>
#include <vector>
inline void series(Base& b) { assert(!b.v.empty()); std::sort(b.v.begin(), b.v.end()); }
