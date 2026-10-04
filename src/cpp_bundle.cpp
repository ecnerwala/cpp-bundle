#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <set>
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

struct Frame {
	FileID fid;
	llvm::StringRef buf;
	unsigned cursor;
	bool system;
};

// Copies the text of every file under root as the preprocessor enters it, replacing user
// #include lines by the included file and dropping #pragma once. Nothing outside root is
// opened (the tool runs with -nostdinc -nostdinc++): a system #include is kept as a line,
// once per header name, unless a prelude header covers it.
class Bundler : public PPCallbacks {
	SourceManager& sm;
	std::string root;
	std::vector<Frame> stack;
	std::string& out;
	std::set<std::string> seen;

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
		return !stack.empty() && sm.getFileID(loc) == stack.back().fid;
	}

public:
	Bundler(SourceManager& sm, std::string root, std::string& out) : sm(sm), root(std::move(root)), out(out) {}

	void InclusionDirective(
		SourceLocation HashLoc, const Token&, llvm::StringRef FileName, bool IsAngled, CharSourceRange,
		OptionalFileEntryRef File, llvm::StringRef, llvm::StringRef, const Module*, bool,
		SrcMgr::CharacteristicKind
	) override {
		if (!onTop(HashLoc)) return;
		auto [b, e] = lineRange(HashLoc);
		Frame& f = stack.back();
		flush(f, b);
		f.cursor = e;
		if (File) return;
		std::string name = FileName.str();
		bool prelude = f.fid == sm.getMainFileID();
		if (!seen.insert(name).second || (!prelude && covered(name))) return;
		out += IsAngled ? "#include <" : "#include \"";
		out += name;
		out += IsAngled ? ">\n" : "\"\n";
	}

	bool FileNotFound(llvm::StringRef) override { return true; }

	void FileChanged(SourceLocation Loc, FileChangeReason Reason, SrcMgr::CharacteristicKind, FileID PrevFID) override {
		if (Reason == EnterFile) {
			FileID fid = sm.getFileID(Loc);
			if (!stack.empty() && stack.back().fid == fid) return;
			bool system = fid == sm.getMainFileID() || !isUserFile(sm.getFileEntryRefForID(fid));
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

	// Whether an already-emitted header makes #include <name> redundant.
	bool covered(llvm::StringRef name) const {
		if (!seen.count("bits/stdc++.h")) return false;
		return isStdHeader(name);
	}

	static bool isStdHeader(llvm::StringRef name) {
		static const std::set<std::string> cxx = {
			"algorithm", "any", "array", "atomic", "barrier", "bit", "bitset", "charconv", "chrono",
			"codecvt", "compare", "complex", "concepts", "condition_variable", "coroutine", "deque",
			"exception", "execution", "expected", "filesystem", "flat_map", "flat_set", "format",
			"forward_list", "fstream", "functional", "future", "generator", "initializer_list",
			"iomanip", "ios", "iosfwd", "iostream", "istream", "iterator", "latch", "limits", "list",
			"locale", "map", "mdspan", "memory", "memory_resource", "mutex", "new", "numbers", "numeric",
			"optional", "ostream", "print", "queue", "random", "ranges", "ratio", "regex", "scoped_allocator",
			"semaphore", "set", "shared_mutex", "source_location", "span", "spanstream", "sstream", "stack",
			"stacktrace", "stdexcept", "stdfloat", "stop_token", "streambuf", "string", "string_view",
			"strstream", "syncstream", "system_error", "thread", "tuple", "typeindex", "typeinfo",
			"type_traits", "unordered_map", "unordered_set", "utility", "valarray", "variant", "vector",
			"version",
		};
		static const std::set<std::string> c = {
			"assert", "complex", "ctype", "errno", "fenv", "float", "inttypes", "iso646", "limits",
			"locale", "math", "setjmp", "signal", "stdalign", "stdarg", "stdatomic", "stdbool", "stddef",
			"stdint", "stdio", "stdlib", "stdnoreturn", "string", "tgmath", "threads", "time", "uchar",
			"wchar", "wctype",
		};
		if (cxx.count(name.str())) return true;
		if (name.consume_front("c") && c.count(name.str())) return true;
		return false;
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
	"usage: cpp-bundle [--root DIR] [--prelude HDR]... FILE... [-- COMPILER_ARGS...]\n"
	"\n"
	"Writes FILEs to stdout with every #include of a file under DIR (default: the current\n"
	"directory) replaced by that file's text. No other file is opened: each other #include\n"
	"is kept as a line, once per header name. COMPILER_ARGS are passed to clang (-std=, -I,\n"
	"-D, ...); the standard include directories are not searched.\n"
	"\n"
	"  --root DIR     inline files under DIR instead of the current directory\n"
	"  --prelude HDR  emit #include <HDR> first; after bits/stdc++.h, standard headers are dropped\n";

} // namespace

int main(int argc, const char** argv) {
	std::vector<std::string> files, args, prelude;
	std::string root = ".";
	bool after = false;
	for (int i = 1; i < argc; i++) {
		llvm::StringRef a = argv[i];
		if (after) args.push_back(argv[i]);
		else if (a == "--") after = true;
		else if (a == "--root" && i + 1 < argc) root = argv[++i];
		else if (a == "--prelude" && i + 1 < argc) prelude.push_back(argv[++i]);
		else if (a == "-h" || a == "--help") {
			llvm::outs() << usage;
			return 0;
		} else if (a.starts_with("-")) {
			llvm::errs() << "cpp-bundle: unknown option " << a << "\n" << usage;
			return 2;
		} else files.push_back(argv[i]);
	}
	if (files.empty()) {
		llvm::errs() << usage;
		return 2;
	}
	std::string mainSrc;
	for (const std::string& h : prelude) mainSrc += "#include <" + h + ">\n";
	for (const std::string& f : files) mainSrc += "#include \"" + realPath(f) + "\"\n";
	const char* mainName = "/cpp-bundle-main.cpp";
	args.insert(args.end(), {"-nostdinc", "-nostdinc++", "-x", "c++"});
	tooling::FixedCompilationDatabase db(".", args);
	tooling::ClangTool tool(db, {mainName});
	tool.mapVirtualFile(mainName, mainSrc);
	std::string out;
	Factory factory;
	factory.root = realPath(root);
	factory.out = &out;
	int rc = tool.run(&factory);
	if (rc != 0) return rc;
	llvm::outs() << out;
	return 0;
}
