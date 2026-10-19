#include <cassert>
#include <cassert>
#include "nonl.hpp"
#include \
	"cont.hpp"
#include "nonl.hpp"
int dup = __LINE__;
#include <cassert>
int after_dropped = __LINE__;
#line 100 "./virtual.cpp"
#include "v.hpp"
int remapped = __LINE__;
int main() { return nonl + cont + v + dup + after_dropped + remapped; }
