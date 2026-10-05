#include <cassert>
// inc/nonl.hpp
#pragma once
int nonl;
// inc/cont.hpp
int cont;
// main.cpp
int dup = __LINE__;
int after_dropped = __LINE__;
// inc/v.hpp
int v;
// ./virtual.cpp
int remapped = __LINE__;
int main() { return nonl + cont + v + dup + after_dropped + remapped; }
// cpp-bundle-main.cpp
int collide;
