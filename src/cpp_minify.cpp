#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#include "minify.h"

const char* usage =
	"usage: cpp-minify [--check] [FILE]\n"
	"\n"
	"Strips comments and unneeded whitespace from FILE (default: stdin) to stdout.\n"
	"\n"
	"  --check  fail unless the output lexes to the same tokens as the input\n";

int main(int argc, const char** argv) {
	bool check = false;
	const char* file = "-";
	for (int i = 1; i < argc; i++) {
		llvm::StringRef a = argv[i];
		if (a == "--check") check = true;
		else if (a == "-h" || a == "--help") {
			llvm::outs() << usage;
			return 0;
		} else if (a.starts_with("-") && a != "-") {
			llvm::errs() << "cpp-minify: unknown option " << a << "\n" << usage;
			return 2;
		} else if (llvm::StringRef(file) != "-") {
			llvm::errs() << usage;
			return 2;
		} else file = argv[i];
	}
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
