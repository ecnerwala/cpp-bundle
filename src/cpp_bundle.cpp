#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

using namespace clang;

namespace {

std::string realPath(llvm::StringRef p) {
	llvm::SmallString<256> out;
	if (llvm::sys::fs::real_path(p, out)) return std::string(p);
	return std::string(out);
}

llvm::StringRef trim(llvm::StringRef s) { return s.trim(" \t\r\n"); }

bool isPragmaOnce(llvm::StringRef line) {
	line = trim(line);
	if (!line.consume_front("#")) return false;
	line = line.ltrim(" \t");
	if (!line.consume_front("pragma")) return false;
	line = line.ltrim(" \t");
	if (!line.consume_front("once")) return false;
	line = line.ltrim(" \t");
	return line.empty() || line.starts_with("//") || line.starts_with("/*");
}

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

// Re-emit the token stream with comments dropped and whitespace reduced to what separates
// tokens; directives keep one space wherever they had whitespace and stay on their own line.
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

struct Frame {
	FileID fid;
	llvm::StringRef buf;
	unsigned cursor;
	bool system;
};

// Copies the text of every file under root as the preprocessor enters it, replacing user
// #include lines by the included file and dropping #pragma once. Files outside root are
// system headers: their bodies are skipped and the #include line is kept (once).
class Bundler : public PPCallbacks {
	SourceManager& sm;
	std::string root;
	std::vector<Frame> stack;
	std::string& out;
	std::vector<std::string> seenSystem;
	bool pendingSystem = false;

	bool isUserFile(OptionalFileEntryRef fe) const {
		if (!fe) return false;
		llvm::StringRef name = fe->getFileEntry().tryGetRealPathName();
		std::string rp = realPath(name.empty() ? fe->getName() : name);
		return llvm::StringRef(rp).starts_with(root + "/");
	}
	void flush(Frame& f, unsigned to) {
		if (!f.system && to > f.cursor) out.append(f.buf.data() + f.cursor, to - f.cursor);
		if (to > f.cursor) f.cursor = to;
	}
	std::pair<unsigned, unsigned> lineRange(SourceLocation loc) {
		Frame& f = stack.back();
		unsigned off = sm.getFileOffset(loc);
		unsigned b = off, e = off;
		while (b > 0 && f.buf[b - 1] != '\n') --b;
		while (e < f.buf.size() && f.buf[e] != '\n') ++e;
		if (e < f.buf.size()) ++e;
		return {b, e};
	}
	bool onTop(SourceLocation loc) const {
		return !stack.empty() && !stack.back().system && sm.getFileID(loc) == stack.back().fid;
	}

public:
	Bundler(SourceManager& sm, std::string root, std::string& out) : sm(sm), root(std::move(root)), out(out) {}

	void InclusionDirective(
		SourceLocation HashLoc, const Token&, llvm::StringRef, bool, CharSourceRange,
		OptionalFileEntryRef File, llvm::StringRef, llvm::StringRef, const Module*, bool,
		SrcMgr::CharacteristicKind
	) override {
		if (!onTop(HashLoc)) return;
		auto [b, e] = lineRange(HashLoc);
		Frame& f = stack.back();
		flush(f, b);
		f.cursor = e;
		if (isUserFile(File)) return;
		std::string line = trim(f.buf.substr(b, e - b)).str();
		if (std::find(seenSystem.begin(), seenSystem.end(), line) == seenSystem.end()) {
			seenSystem.push_back(line);
			out += line;
			out += '\n';
		}
		pendingSystem = File.has_value();
	}

	void FileChanged(SourceLocation Loc, FileChangeReason Reason, SrcMgr::CharacteristicKind, FileID PrevFID) override {
		if (Reason == EnterFile) {
			FileID fid = sm.getFileID(Loc);
			if (!stack.empty() && stack.back().fid == fid) return;
			bool system;
			if (fid == sm.getMainFileID()) system = false;
			else if (!sm.getFileEntryRefForID(fid)) system = true;
			else system = stack.empty() || stack.back().system || pendingSystem;
			pendingSystem = false;
			stack.push_back({fid, sm.getBufferData(fid), 0, system});
		} else if (Reason == ExitFile) {
			if (PrevFID.isInvalid()) return;
			if (stack.empty() || stack.back().fid != PrevFID) {
				llvm::errs() << "cpp-bundle: unbalanced file exit\n";
				std::exit(1);
			}
			flush(stack.back(), stack.back().buf.size());
			stack.pop_back();
		}
	}

	void FileSkipped(const FileEntryRef&, const Token&, SrcMgr::CharacteristicKind) override {
		pendingSystem = false;
	}

	void PragmaDirective(SourceLocation Loc, PragmaIntroducerKind Introducer) override {
		if (Introducer != PIK_HashPragma || !onTop(Loc)) return;
		auto [b, e] = lineRange(Loc);
		Frame& f = stack.back();
		if (isPragmaOnce(f.buf.substr(b, e - b))) {
			flush(f, b);
			f.cursor = e;
		}
	}

	void EndOfMainFile() override {
		while (!stack.empty()) {
			flush(stack.back(), stack.back().buf.size());
			stack.pop_back();
		}
	}
};

class Action : public PreprocessOnlyAction {
	std::string root;
	std::string* out;

public:
	Action(std::string root, std::string* out) : root(std::move(root)), out(out) {}
	void ExecuteAction() override {
		CompilerInstance& ci = getCompilerInstance();
		ci.getPreprocessor().addPPCallbacks(std::make_unique<Bundler>(ci.getSourceManager(), root, *out));
		PreprocessOnlyAction::ExecuteAction();
	}
};

struct Factory : tooling::FrontendActionFactory {
	std::string root;
	std::string* out;
	std::unique_ptr<FrontendAction> create() override { return std::make_unique<Action>(root, out); }
};

const char* usage =
	"usage: cpp-bundle [--minify] [--root DIR] FILE [-- COMPILER_ARGS...]\n"
	"\n"
	"Writes FILE to stdout with every #include of a file under DIR (default: the current\n"
	"directory) replaced by that file's text; other #include lines are kept, each once.\n"
	"COMPILER_ARGS are passed to clang (-std=, -I, -D, ...).\n"
	"\n"
	"  --minify    strip comments and unneeded whitespace from the result\n"
	"  --root DIR  inline files under DIR instead of the current directory\n";

} // namespace

int main(int argc, const char** argv) {
	std::vector<std::string> files, args;
	std::string root = ".";
	bool after = false, doMinify = false;
	for (int i = 1; i < argc; i++) {
		llvm::StringRef a = argv[i];
		if (after) args.push_back(argv[i]);
		else if (a == "--") after = true;
		else if (a == "--minify") doMinify = true;
		else if (a == "--root" && i + 1 < argc) root = argv[++i];
		else if (a == "-h" || a == "--help") {
			llvm::outs() << usage;
			return 0;
		} else if (a.starts_with("-")) {
			llvm::errs() << "cpp-bundle: unknown option " << a << "\n" << usage;
			return 2;
		} else files.push_back(argv[i]);
	}
	if (files.size() != 1) {
		llvm::errs() << usage;
		return 2;
	}
	if (std::none_of(args.begin(), args.end(), [](const std::string& s) { return llvm::StringRef(s).starts_with("-resource-dir"); })) {
		args.push_back("-resource-dir=" CPP_BUNDLE_RESOURCE_DIR);
	}
	tooling::FixedCompilationDatabase db(".", args);
	tooling::ClangTool tool(db, files);
	std::string out;
	Factory factory;
	factory.root = realPath(root);
	factory.out = &out;
	int rc = tool.run(&factory);
	if (rc != 0) return rc;
	llvm::outs() << (doMinify ? minify(out) : out);
	return 0;
}
