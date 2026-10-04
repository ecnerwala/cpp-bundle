#include "minify.h"

#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Token.h>

using namespace clang;

namespace {

LangOptions cxxLangOpts() {
	LangOptions lo;
	lo.CPlusPlus = lo.CPlusPlus11 = lo.CPlusPlus14 = lo.CPlusPlus17 = lo.CPlusPlus20 = lo.CPlusPlus23 = 1;
	lo.LineComment = lo.Bool = lo.Digraphs = lo.CXXOperatorNames = lo.WChar = 1;
	return lo;
}

// Whether a and b still lex as those two tokens when written without a separator.
bool needSpace(llvm::StringRef a, llvm::StringRef b) {
	std::string joined = (a + b).str();
	Lexer lex(SourceLocation(), cxxLangOpts(), joined.data(), joined.data(), joined.data() + joined.size());
	lex.SetCommentRetentionState(false);
	Token t1, t2, t3;
	lex.LexFromRawLexer(t1);
	lex.LexFromRawLexer(t2);
	lex.LexFromRawLexer(t3);
	return !(t1.getLength() == a.size() && t2.getLength() == b.size() && t3.is(tok::eof));
}

} // namespace

std::string minify(llvm::StringRef code) {
	LangOptions lo = cxxLangOpts();
	Lexer lex(SourceLocation(), lo, code.data(), code.data(), code.data() + code.size());
	lex.SetCommentRetentionState(false);
	std::string out;
	Token tok;
	bool inDirective = false;
	const char* prevEnd = code.data();
	llvm::StringRef prev;
	while (true) {
		lex.LexFromRawLexer(tok);
		if (tok.is(tok::eof)) break;
		const char* end = lex.getBufferLocation();
		llvm::StringRef text(end - tok.getLength(), tok.getLength());
		bool sol = tok.isAtStartOfLine();
		if (sol && inDirective) inDirective = false;
		if (sol && tok.is(tok::hash)) inDirective = true;
		if (!out.empty()) {
			if (sol) out += '\n';
			else if (inDirective ? text.data() > prevEnd : needSpace(prev, text)) out += ' ';
		}
		out += text;
		prevEnd = end;
		prev = text;
	}
	if (!out.empty()) out += '\n';
	return out;
}

std::vector<std::string> tokens(llvm::StringRef code) {
	Lexer lex(SourceLocation(), cxxLangOpts(), code.data(), code.data(), code.data() + code.size());
	lex.SetCommentRetentionState(false);
	std::vector<std::string> out;
	Token tok;
	while (true) {
		lex.LexFromRawLexer(tok);
		if (tok.is(tok::eof)) break;
		out.emplace_back(lex.getBufferLocation() - tok.getLength(), tok.getLength());
	}
	return out;
}
