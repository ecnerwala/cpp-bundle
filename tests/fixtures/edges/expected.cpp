#include <cassert>
#line 1 "inc/nonl.hpp"
#pragma once /* kept:
   unterminated on its line */
int nonl; // no newline after this comment
#line 1 "inc/cont.hpp"

int cont;
#line 7 "main.cpp"
int dup = __LINE__;
#line 9 "main.cpp"
int after_dropped = __LINE__;
#line 100 "./virtual.cpp"
#line 1 "inc/v.hpp"
int v;
#line 101 "./virtual.cpp"
int remapped = __LINE__;
int main() { return nonl + cont + v + dup + after_dropped + remapped; }
#line 1 "cpp-bundle-main.cpp"
int collide;
