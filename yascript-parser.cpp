#include "yascript-parser.hpp"

#include <charconv>
#include <sstream>

namespace yascript {

static bool isCommentToken(std::string_view token) {
    return token.empty() || token.front() == '#' || token == "//";
}

static bool parseInteger(std::string_view token, long long &value, std::string &error) {
    const char *begin = token.data();
    const char *end = token.data() + token.size();
    auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        error = "invalid integer: '" + std::string(token) + "'";
        return false;
    }
    return true;
}

ParseResult parseProgram(std::string_view source) {
    ParseResult result;
    std::string sourceCopy(source);
    std::istringstream stream(sourceCopy);
    std::string token;
    std::vector<unsigned long long> loopStack;

    while (stream >> token) {
        if (isCommentToken(token)) {
            std::string rest;
            std::getline(stream, rest);
            continue;
        }

        if (token == "left") {
            result.instructions.push_back({OpCode::Left, 0, 0});
        } else if (token == "rght") {
            result.instructions.push_back({OpCode::Right, 0, 0});
        } else if (token == "add") {
            result.instructions.push_back({OpCode::Add, 0, 0});
        } else if (token == "min") {
            result.instructions.push_back({OpCode::Min, 0, 0});
        } else if (token == "set") {
            if (!(stream >> token)) {
                result.error = "missing argument for 'set'";
                return result;
            }
            long long value;
            if (!parseInteger(token, value, result.error) || value < 0) {
                if (result.error.empty()) {
                    result.error = "'set' requires a non-negative integer";
                }
                return result;
            }
            result.instructions.push_back({OpCode::Set, static_cast<unsigned long long>(value), 0});
        } else if (token == "goto") {
            if (!(stream >> token)) {
                result.error = "missing argument for 'goto'";
                return result;
            }
            long long value;
            if (!parseInteger(token, value, result.error) || value < 0) {
                if (result.error.empty()) {
                    result.error = "'goto' requires a non-negative integer";
                }
                return result;
            }
            result.instructions.push_back({OpCode::Goto, static_cast<unsigned long long>(value), 0});
        } else if (token == "output") {
            result.instructions.push_back({OpCode::Output, 0, 0});
        } else if (token == "read") {
            result.instructions.push_back({OpCode::Read, 0, 0});
        } else if (token == "print") {
            result.instructions.push_back({OpCode::Print, 0, 0});
        } else if (token == "zero") {
            result.instructions.push_back({OpCode::Zero, 0, 0});
        } else if (token == "loop") {
            if (!(stream >> token)) {
                result.error = "missing argument for 'loop'";
                return result;
            }
            long long value;
            if (!parseInteger(token, value, result.error) || value < 0) {
                if (result.error.empty()) {
                    result.error = "'loop' requires a non-negative integer";
                }
                return result;
            }
            unsigned long long index = result.instructions.size();
            result.instructions.push_back({OpCode::LoopStart, static_cast<unsigned long long>(value), 0});
            loopStack.push_back(index);
        } else if (token == "end") {
            if (loopStack.empty()) {
                result.error = "'end' without matching 'loop'";
                return result;
            }
            unsigned long long startIndex = loopStack.back();
            loopStack.pop_back();
            unsigned long long endIndex = result.instructions.size();
            result.instructions.push_back({OpCode::LoopEnd, 0, startIndex});
            result.instructions[startIndex].target = endIndex;
        } else {
            result.error = "unknown keyword: '" + token + "'";
            return result;
        }
    }

    if (!loopStack.empty()) {
        result.error = "missing 'end' for 'loop'";
        return result;
    }

    return result;
}

} // namespace yascript
