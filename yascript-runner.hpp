#ifndef YASCRIPT_RUNNER_HPP
#define YASCRIPT_RUNNER_HPP

#include <iostream>
#include <string>
#include <vector>
#include "yascript-parser.hpp"

namespace yascript {

struct RunResult {
    bool success;
    std::string error;
};

RunResult runProgram(const std::vector<Instruction> &program, std::istream &input, std::ostream &output);

} // namespace yascript

#endif // YASCRIPT_RUNNER_HPP
