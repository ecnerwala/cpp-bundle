#pragma once

#include <llvm/ADT/StringRef.h>

#include <string>
#include <vector>

// Re-emits C++ source with comments dropped and whitespace reduced to what separates
// tokens; one line of tokens per source line, directives keep one space where they had
// whitespace. Identifiers are not renamed.
std::string minify(llvm::StringRef code);

// Token spellings of code, as the raw lexer sees them (comments excluded).
std::vector<std::string> tokens(llvm::StringRef code);
