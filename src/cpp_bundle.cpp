#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Driver/Options.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/OptTable.h>
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
	bool copy;
	bool opaque;
};

// Copies the text of every file reached through #include "..." as the preprocessor enters
// it, replacing the directive by the file and dropping #pragma once. #include <...> is kept
// as a line, once per header name, unless a prelude header covers it; the standard include
// directories are never searched (-nostdinc -nostdinc++). -include files come first: inlined
// if they exist, emitted as #include <...> otherwise.
class Bundler : public PPCallbacks {
	Preprocessor& pp;
	SourceManager& sm;
	std::vector<Frame> stack;
	std::string& out;
	std::set<std::string> seen;
	bool nextOpaque = false;

	void flush(Frame& f, unsigned to) {
		if (f.copy && to > f.cursor) out.append(f.buf.data() + f.cursor, to - f.cursor);
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
		return !stack.empty() && sm.getFileID(loc) == stack.back().fid && !stack.back().opaque;
	}

public:
	Bundler(Preprocessor& pp, std::string& out) : pp(pp), sm(pp.getSourceManager()), out(out) {}

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
		bool prelude = f.fid == pp.getPredefinesFileID();
		if (!IsAngled && File) return;
		if (!IsAngled && !prelude) {
			PresumedLoc loc = sm.getPresumedLoc(HashLoc);
			llvm::errs() << "cpp-bundle: " << loc.getFilename() << ":" << loc.getLine() << ": file not found: \"" << FileName << "\"\n";
			std::exit(1);
		}
		nextOpaque = File.has_value();
		std::string name = FileName.str();
		if (!seen.insert(name).second || (!prelude && covered(name))) return;
		out += "#include <" + name + ">\n";
	}

	bool FileNotFound(llvm::StringRef) override { return true; }

	void FileChanged(SourceLocation Loc, FileChangeReason Reason, SrcMgr::CharacteristicKind, FileID PrevFID) override {
		if (Reason == EnterFile) {
			FileID fid = sm.getFileID(Loc);
			if (!stack.empty() && stack.back().fid == fid) return;
			bool special = fid == sm.getMainFileID() || fid == pp.getPredefinesFileID();
			bool opaque = !special && (nextOpaque || (!stack.empty() && stack.back().opaque));
			nextOpaque = false;
			stack.push_back({fid, sm.getBufferData(fid), 0, !special && !opaque, opaque});
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
	std::string* out;

public:
	explicit Action(std::string* out) : out(out) {}
	void ExecuteAction() override {
		Preprocessor& pp = getCompilerInstance().getPreprocessor();
		pp.addPPCallbacks(std::make_unique<Bundler>(pp, *out));
		PreprocessOnlyAction::ExecuteAction();
	}
};

struct Factory : tooling::FrontendActionFactory {
	std::string* out;
	std::unique_ptr<FrontendAction> create() override { return std::make_unique<Action>(out); }
};

const char* usage =
	"usage: cpp-bundle [CLANG_ARGS...] FILE...\n"
	"\n"
	"Writes FILEs to stdout with every #include \"...\" replaced by that file's text, as the\n"
	"preprocessor sees it. #include <...> is kept as a line, once per header name; the standard\n"
	"include directories are never searched. CLANG_ARGS are clang's (-std=, -I, -D, ...);\n"
	"-include HDR puts HDR first: inlined if it is a file, #include <HDR> otherwise, and after\n"
	"bits/stdc++.h later includes of standard headers are dropped.\n";

} // namespace

int main(int argc, const char** argv) {
	for (int i = 1; i < argc; i++) {
		llvm::StringRef a = argv[i];
		if (a == "-h" || a == "--help") {
			llvm::outs() << usage;
			return 0;
		}
	}
	unsigned missingIndex, missingCount;
	llvm::opt::InputArgList parsed = driver::getDriverOptTable().ParseArgs(
		llvm::ArrayRef(argv + 1, argc - 1), missingIndex, missingCount);
	if (missingCount) {
		llvm::errs() << "cpp-bundle: missing argument to " << parsed.getArgString(missingIndex) << "\n";
		return 2;
	}
	std::vector<std::string> files, args;
	std::set<unsigned> inputs;
	for (const llvm::opt::Arg* a : parsed)
		if (a->getOption().matches(driver::options::OPT_INPUT)) inputs.insert(a->getIndex());
	for (int i = 1; i < argc; i++) (inputs.count(i - 1) ? files : args).push_back(argv[i]);
	if (files.empty()) {
		llvm::errs() << usage;
		return 2;
	}
	std::string mainSrc;
	for (const std::string& f : files) {
		if (!llvm::sys::fs::exists(f)) {
			llvm::errs() << "cpp-bundle: file not found: " << f << "\n";
			return 1;
		}
		mainSrc += "#include \"" + realPath(f) + "\"\n";
	}
	const char* mainName = "/cpp-bundle-main.cpp";
	args.insert(args.end(), {"-nostdinc", "-nostdinc++", "-x", "c++"});
	tooling::FixedCompilationDatabase db(".", args);
	tooling::ClangTool tool(db, {mainName});
	tool.mapVirtualFile(mainName, mainSrc);
	std::string out;
	Factory factory;
	factory.out = &out;
	int rc = tool.run(&factory);
	if (rc != 0) return rc;
	llvm::outs() << out;
	return 0;
}
