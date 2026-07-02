#include "yascript-interface.hpp"
#include "yascript-parser.hpp"
#include "yascript-runner.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>

namespace yascript {

static bool hasYsExtension(const std::string &path) {
    std::string_view view(path);
    return view.size() > 3 && view.substr(view.size() - 3) == ".ys";
}

int runSource(const std::string &source, std::istream &input, std::ostream &output) {
    ParseResult parsed = parseProgram(source);
    if (!parsed.error.empty()) {
        std::cerr << "parse error: " << parsed.error << '\n';
        return 1;
    }

    RunResult result = runProgram(parsed.instructions, input, output);
    if (!result.success) {
        std::cerr << "runtime error: " << result.error << '\n';
        return 2;
    }

    return 0;
}

int runFile(const std::string &path, std::istream &input, std::ostream &output) {
    if (!hasYsExtension(path)) {
        std::cerr << "unsupported file extension: only .ys files are allowed\n";
        return 1;
    }

    std::ifstream file(path);
    if (!file) {
        std::cerr << "cannot open file: " << path << '\n';
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return runSource(source, input, output);
}

} // namespace yascript

static void printUsage() {
    std::cout << "yascript - simplified Brainfuck-style tape language\n"
              << "usage: yascript [-e code] [file.ys]" << '\n'
              << "keywords: left, rght, add, min, set <n>, goto <n>, output, print, read, zero, loop, end" << '\n';
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        printUsage();
        return 0;
    }

    if (std::string(argv[1]) == "-e") {
        if (argc < 3) {
            std::cerr << "option -e requires source text\n";
            return 1;
        }
        return yascript::runSource(argv[2]);
    }

    return yascript::runFile(argv[1]);
}
