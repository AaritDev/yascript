#ifndef YASCRIPT_PARSER_HPP
#define YASCRIPT_PARSER_HPP

#include <string>
#include <string_view>
#include <vector>

namespace yascript {

enum class OpCode {
    Left,
    Right,
    Add,
    Min,
    Set,
    Goto,
    Output,
    Read,
    Print,
    Zero,
    LoopStart,
    LoopEnd,
};

struct Instruction {
    OpCode op;
    unsigned long long arg;
    unsigned long long target;
};

struct ParseResult {
    std::vector<Instruction> instructions;
    std::string error;
};

ParseResult parseProgram(std::string_view source);

} // namespace yascript

#endif // YASCRIPT_PARSER_HPP
