#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Token.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <string>
#include <vector>

using namespace clang;

namespace {

// What clang uses for -std=c++23.
LangOptions cxxLangOpts() {
	LangOptions lo;
	std::vector<std::string> includes;
	LangOptions::setLangDefaults(
		lo, Language::CXX, llvm::Triple(llvm::sys::getDefaultTargetTriple()), includes, LangStandard::lang_gnucxx29);
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

struct Tok {
	llvm::StringRef text;
	tok::TokenKind kind;
	bool sol;
};

std::vector<Tok> lexAll(llvm::StringRef code) {
	Lexer lex(SourceLocation(), cxxLangOpts(), code.data(), code.data(), code.data() + code.size());
	lex.SetCommentRetentionState(false);
	std::vector<Tok> out;
	Token tok;
	while (true) {
		lex.LexFromRawLexer(tok);
		if (tok.is(tok::eof)) break;
		const char* end = lex.getBufferLocation();
		out.push_back({llvm::StringRef(end - tok.getLength(), tok.getLength()), tok.getKind(), tok.isAtStartOfLine()});
	}
	return out;
}

size_t lineEnd(const std::vector<Tok>& toks, size_t i) {
	while (++i < toks.size() && !toks[i].sol) {}
	return i;
}

// The file named by the #line directive starting at toks[i], or empty if that is not one.
llvm::StringRef lineDirectiveFile(const std::vector<Tok>& toks, size_t i) {
	if (!toks[i].sol || toks[i].kind != tok::hash) return "";
	if (i + 1 >= toks.size() || toks[i + 1].sol || toks[i + 1].text != "line") return "";
	for (size_t j = i + 2, e = lineEnd(toks, i); j < e; j++)
		if (toks[j].kind == tok::string_literal) return toks[j].text.drop_front().drop_back();
	return "";
}

std::string unescape(llvm::StringRef s) {
	std::string r;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '\\' && i + 1 < s.size()) i++;
		r += s[i];
	}
	return r;
}

// Re-emits the token stream with comments dropped and whitespace reduced to what separates
// tokens; directives keep one space wherever they had whitespace and stay on their own line.
// #line directives become one "// file" line per run of lines from the same file.
std::string minify(llvm::StringRef code) {
	std::vector<Tok> toks = lexAll(code);
	std::string out;
	llvm::StringRef file, pending;
	for (size_t i = 0; i < toks.size(); i++) {
		if (llvm::StringRef f = lineDirectiveFile(toks, i); !f.empty()) {
			pending = f;
			i = lineEnd(toks, i) - 1;
			continue;
		}
		const Tok& t = toks[i];
		if (t.sol && !pending.empty()) {
			if (pending != file) {
				file = pending;
				if (!out.empty()) out += '\n';
				out += "// " + unescape(file);
			}
			pending = "";
		}
		if (!out.empty()) {
			if (t.sol) out += '\n';
			else {
				size_t j = i;
				while (!toks[j].sol) --j;
				bool directive = toks[j].kind == tok::hash;
				const Tok& p = toks[i - 1];
				if (directive ? t.text.data() > p.text.end() : needSpace(p.text, t.text)) out += ' ';
			}
		}
		out += t.text;
	}
	if (!out.empty()) out += '\n';
	return out;
}

std::vector<std::string> tokens(llvm::StringRef code) {
	std::vector<Tok> toks = lexAll(code);
	std::vector<std::string> out;
	for (size_t i = 0; i < toks.size(); i++) {
		if (!lineDirectiveFile(toks, i).empty()) {
			i = lineEnd(toks, i) - 1;
			continue;
		}
		out.push_back(toks[i].text.str());
	}
	return out;
}

} // namespace

const char* usage =
	"usage: cpp-minify [--check] [FILE]\n"
	"\n"
	"Strips comments and unneeded whitespace from FILE (default: stdin) to stdout.\n"
	"\n"
	"  --check  fail unless the output lexes to the same tokens as the input\n";

int main(int argc, const char** argv) {
	bool check = false;
	const char* file = nullptr;
	for (int i = 1; i < argc; i++) {
		llvm::StringRef a = argv[i];
		if (a == "--check") check = true;
		else if (a == "-h" || a == "--help") {
			llvm::outs() << usage;
			return 0;
		} else if (a.starts_with("-") && a != "-") {
			llvm::errs() << "cpp-minify: unknown option " << a << "\n" << usage;
			return 2;
		} else if (file) {
			llvm::errs() << usage;
			return 2;
		} else file = argv[i];
	}
	if (!file) file = "-";
	auto buf = llvm::MemoryBuffer::getFileOrSTDIN(file);
	if (!buf) {
		llvm::errs() << "cpp-minify: " << file << ": " << buf.getError().message() << "\n";
		return 1;
	}
	llvm::StringRef in = (*buf)->getBuffer();
	std::string out = minify(in);
	if (check) {
		std::vector<std::string> a = tokens(in), b = tokens(out);
		for (size_t i = 0; i < a.size() || i < b.size(); i++) {
			if (i < a.size() && i < b.size() && a[i] == b[i]) continue;
			llvm::errs() << "cpp-minify: token " << i << " differs: "
			             << (i < a.size() ? a[i] : "<eof>") << " vs " << (i < b.size() ? b[i] : "<eof>") << "\n";
			return 1;
		}
	}
	llvm::outs() << out;
	return 0;
}
