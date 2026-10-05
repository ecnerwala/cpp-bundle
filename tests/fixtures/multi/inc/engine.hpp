#pragma once
#include "base.hpp"
#include <utility>
inline Base engine(Base b) { return std::move(b); }
