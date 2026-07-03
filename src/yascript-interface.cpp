#include "../include/yascript-interface.hpp"
#include "../include/yascript-parser.hpp"
#include "../include/yascript-runner.hpp"

#include <fstream>
#include <string_view>

namespace yascript {

int runSource(const std::string& source, std::istream& input, std::ostream& output) {
    ParseResult parsed = parseProgram(source);
    if (!parsed.error.empty()) {
        output << "parse error: " << parsed.error << '\n';
        return 1;
    }

    RunResult result = runProgram(parsed.instructions, input, output, source);
    if (!result.success) {
        output << "runtime error: " << result.error << '\n';
        return 2;
    }

    return 0;
}

int runFile(const std::string& path, std::istream& input, std::ostream& output) {
    if (path.size() < 3 || path.substr(path.size() - 3) != ".ys") {
        output << "error: only .ys files are supported\n";
        return 1;
    }

    std::ifstream file(path);
    if (!file) {
        output << "error: cannot open file: " << path << '\n';
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return runSource(source, input, output);
}

} // namespace yascript

static void printUsage(std::ostream& out) {
    out << "yascript - fast, optimizing tape interpreter\n"
        << "usage: yascript [-e code] [file.ys]\n"
        << "commands: left rght add sub set goto output print read zero repeat end ;\n"
        << "simple statements end with ';' or a newline\n";
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        printUsage(std::cout);
        return 0;
    }

    if (std::string(argv[1]) == "-e") {
        if (argc < 3) {
            std::cerr << "error: -e requires source code\n";
            return 1;
        }
        return yascript::runSource(argv[2]);
    }

    return yascript::runFile(argv[1]);
}
