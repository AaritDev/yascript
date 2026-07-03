#ifndef YASCRIPT_PARSER_HPP
#define YASCRIPT_PARSER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include "yascript-lexer.hpp"

namespace yascript {

enum class OpCode : uint8_t {
    Left,
    Right,
    Add,
    Sub,
    Set,
    Goto,
    Output,
    Read,
    Print,
    Zero,
    RepeatStart,
    RepeatEnd,
};

struct Instruction {
    OpCode op;
    uint64_t arg;
    uint64_t target;
    uint32_t line;
    uint32_t column;
};

struct ParseResult {
    std::vector<Instruction> instructions;
    std::string error;
};

ParseResult parseProgram(std::string_view source);

} // namespace yascript

#endif // YASCRIPT_PARSER_HPP
