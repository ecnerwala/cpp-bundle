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

// What clang uses for -std=gnu++2d.
const LangOptions& cxxLangOpts() {
	static const LangOptions lo = [] {
		LangOptions lo;
		std::vector<std::string> includes;
		LangOptions::setLangDefaults(
			lo, Language::CXX, llvm::Triple(llvm::sys::getDefaultTargetTriple()), includes, LangStandard::lang_gnucxx29);
		return lo;
	}();
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
	bool sol;        // first token on its line (comments do not count)
	std::string ws;  // whitespace since the previous token, comments removed
	bool comment;    // a comment was removed from ws
};

std::vector<Tok> lexAll(llvm::StringRef code) {
	Lexer lex(SourceLocation(), cxxLangOpts(), code.data(), code.data(), code.data() + code.size());
	lex.SetCommentRetentionState(true);
	std::vector<Tok> out;
	Token tok;
	const char* prev = code.data();
	Tok cur{};
	while (true) {
		lex.LexFromRawLexer(tok);
		if (tok.is(tok::eof)) break;
		const char* end = lex.getBufferLocation();
		const char* begin = end - tok.getLength();
		cur.ws.append(prev, begin);
		cur.sol |= tok.isAtStartOfLine();
		prev = end;
		if (tok.is(tok::comment)) {
			cur.comment = true;
			continue;
		}
		cur.text = llvm::StringRef(begin, tok.getLength());
		cur.kind = tok.getKind();
		out.push_back(std::move(cur));
		cur = {};
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

enum class Level { light, medium, full };

struct Line {
	std::string text;
	bool alone;  // directive or "// file" marker: stays on its own line
	llvm::StringRef first, last;
};

// One Line per source line, comments dropped. light keeps the line's own whitespace
// (indentation, spacing) and only drops trailing whitespace; otherwise whitespace is reduced
// to what separates tokens, except that directives keep one space wherever they had some.
// #line directives become one "// file" line per run of lines from the same file.
std::vector<Line> toLines(const std::vector<Tok>& toks, Level level) {
	std::vector<Line> out;
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
				out.push_back({"// " + unescape(file), true, "", ""});
			}
			pending = "";
		}
		if (t.sol || out.empty()) {
			out.push_back({"", t.kind == tok::hash, t.text, t.text});
			if (level == Level::light)
				if (size_t nl = t.ws.rfind('\n'); nl != std::string::npos) out.back().text = t.ws.substr(nl + 1);
		} else {
			Line& l = out.back();
			const Tok& p = toks[i - 1];
			if (level == Level::light) {
				if (!t.comment || t.ws.find('\n') != std::string::npos) l.text += t.ws;
				else if (!t.ws.empty() || needSpace(p.text, t.text)) l.text += ' ';
			} else if (l.alone ? t.text.data() > p.text.end() : needSpace(p.text, t.text)) l.text += ' ';
		}
		out.back().text += t.text;
		out.back().last = t.text;
	}
	return out;
}

// full packs consecutive non-directive lines onto shared lines of up to width characters.
std::string join(const std::vector<Line>& lines, Level level, size_t width) {
	std::string out, packed;
	llvm::StringRef last;
	auto flush = [&] {
		if (packed.empty()) return;
		out += packed + "\n";
		packed.clear();
	};
	for (const Line& l : lines) {
		if (level != Level::full || l.alone) {
			flush();
			out += l.text + "\n";
			continue;
		}
		if (!packed.empty() && packed.size() + 1 + l.text.size() > width) flush();
		if (!packed.empty() && needSpace(last, l.first)) packed += ' ';
		packed += l.text;
		last = l.last;
		if (l.text.find('\n') != std::string::npos) flush();
	}
	flush();
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
	"usage: cpp-minify [--level light|medium|full] [--width N] [--check] [FILE]\n"
	"\n"
	"Strips comments and unneeded whitespace from FILE (default: stdin) to stdout.\n"
	"\n"
	"  --level light   drop comments, blank lines and trailing whitespace only\n"
	"  --level medium  also indentation and spaces between tokens (default)\n"
	"  --level full    also pack statements onto lines of up to --width (120) characters\n"
	"  --check         fail unless the output lexes to the same tokens as the input\n";

int main(int argc, const char** argv) {
	bool check = false;
	Level level = Level::medium;
	size_t width = 120;
	const char* file = nullptr;
	for (int i = 1; i < argc; i++) {
		llvm::StringRef a = argv[i];
		llvm::StringRef value;
		bool hasValue = (a == "--level" || a == "--width") && i + 1 < argc;
		if (hasValue) value = argv[++i];
		if (a == "--check") check = true;
		else if (a == "--level" && value == "light") level = Level::light;
		else if (a == "--level" && value == "medium") level = Level::medium;
		else if (a == "--level" && value == "full") level = Level::full;
		else if (a == "--width" && !value.getAsInteger(10, width) && width > 0) {}
		else if (a == "-h" || a == "--help") {
			llvm::outs() << usage;
			return 0;
		} else if (a.starts_with("-") && a != "-") {
			llvm::errs() << "cpp-minify: bad option " << a << (hasValue ? " " : "") << value << "\n" << usage;
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
	std::string out = join(toLines(lexAll(in), level), level, width);
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
