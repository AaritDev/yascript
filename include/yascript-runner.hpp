#ifndef YASCRIPT_RUNNER_HPP
#define YASCRIPT_RUNNER_HPP

#include "yascript-parser.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace yascript {

struct RunResult {
    bool success;
    std::string error;
};

RunResult runProgram(const std::vector<Instruction>& program, std::istream& input, std::ostream& output, std::string_view source = "");

} // namespace yascript

#endif // YASCRIPT_RUNNER_HPP
