#include "../include/yascript-parser.hpp"
#include "../include/yascript-lexer.hpp"
#include "../include/yascript-diagnostics.hpp"

#include <charconv>
#include <limits>

namespace yascript {

namespace {

bool isSeparator(TokenType type) {
    return type == TokenType::Semicolon || type == TokenType::Newline;
}

bool checkedAdd(uint64_t lhs, uint64_t rhs, uint64_t& result) {
    if (lhs > std::numeric_limits<uint64_t>::max() - rhs) {
        return false;
    }
    result = lhs + rhs;
    return true;
}

bool checkedMultiply(uint64_t lhs, uint64_t rhs, uint64_t& result) {
    if (lhs != 0 && rhs > std::numeric_limits<uint64_t>::max() / lhs) {
        return false;
    }
    result = lhs * rhs;
    return true;
}

bool isArithmeticOp(OpCode op) {
    return op == OpCode::Add || op == OpCode::Sub || op == OpCode::Left || op == OpCode::Right;
}

bool isFoldableRepeatOp(OpCode op) {
    return op == OpCode::Add || op == OpCode::Sub || op == OpCode::Left ||
           op == OpCode::Right || op == OpCode::Set || op == OpCode::Zero;
}

} // namespace

class Parser {
private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::string error_;
    std::string_view source_;

    const Token& current() const {
        return pos_ < tokens_.size() ? tokens_[pos_] : tokens_.back();
    }

    void advance() {
        if (pos_ < tokens_.size()) {
            ++pos_;
        }
    }

    void setError(const std::string& message) {
        if (error_.empty()) {
            error_ = message;
        }
    }

    std::string syntaxError(const std::string& message) const {
        return syntaxError(message, current());
    }

    std::string syntaxError(const std::string& message, const Token& token) const {
        std::string fullMsg = message + " at line " + std::to_string(token.line) +
                              ", column " + std::to_string(token.column);
        size_t len = token.text.empty() ? 1 : token.text.length();
        return formatDiagnostic(source_, token.line, token.column, len, fullMsg);
    }

    Instruction makeInstruction(OpCode op, uint64_t arg, uint64_t target, const Token& token) const {
        return Instruction{op, arg, target, token.line, token.column};
    }

    std::string tokenTextForError(const Token& token) const {
        if (token.type == TokenType::End) {
            return "end of file";
        }
        if (token.type == TokenType::Newline) {
            return "newline";
        }
        return "'" + std::string(token.text) + "'";
    }

    std::string lexicalErrorFor(const Token& token) const {
        const std::string text(token.text);
        std::string message = "unknown token '" + text + "' at line " + std::to_string(token.line) +
                              ", column " + std::to_string(token.column);

        if (text == "right") {
            message += "; did you mean 'rght'?";
        } else if (text == "loop") {
            message += "; did you mean 'repeat'?";
        } else if (text == "min" || text == "minus") {
            message += "; did you mean 'sub'?";
        } else if (text == "prints") {
            message += "; did you mean 'print'?";
        }

        size_t len = token.text.empty() ? 1 : token.text.length();
        return formatDiagnostic(source_, token.line, token.column, len, message);
    }

    bool parseNumber(uint64_t& value, const std::string& commandName) {
        const Token& token = current();
        if (token.type != TokenType::Number) {
            std::string msg = commandName + " expects a number argument, got " + tokenTextForError(token) +
                             " at line " + std::to_string(token.line) +
                             ", column " + std::to_string(token.column);
            size_t len = token.text.empty() ? 1 : token.text.length();
            setError(formatDiagnostic(source_, token.line, token.column, len, msg));
            return false;
        }

        value = 0;
        const char* begin = token.text.data();
        const char* end = begin + token.text.size();
        auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end) {
            std::string msg = commandName + " number is out of uint64 range at line " +
                             std::to_string(token.line) + ", column " + std::to_string(token.column);
            size_t len = token.text.empty() ? 1 : token.text.length();
            setError(formatDiagnostic(source_, token.line, token.column, len, msg));
            return false;
        }

        advance();
        return true;
    }

    void rebuildRepeatLinks(std::vector<Instruction>& instructions) const {
        std::vector<size_t> stack;
        stack.reserve(instructions.size());

        for (size_t i = 0; i < instructions.size(); ++i) {
            auto& instr = instructions[i];
            if (instr.op == OpCode::RepeatStart) {
                stack.push_back(i);
            } else if (instr.op == OpCode::RepeatEnd && !stack.empty()) {
                size_t startIndex = stack.back();
                stack.pop_back();
                instructions[startIndex].target = i;
                instr.target = startIndex;
            }
        }
    }

    bool appendInstruction(std::vector<Instruction>& optimized, Instruction instr, bool& changed) {
        if (instr.op == OpCode::Set && instr.arg == 0) {
            instr.op = OpCode::Zero;
        }

        if ((instr.op == OpCode::Add || instr.op == OpCode::Sub ||
             instr.op == OpCode::Left || instr.op == OpCode::Right) &&
            instr.arg == 0) {
            changed = true;
            return true;
        }

        if (!optimized.empty()) {
            auto& last = optimized.back();

            if (last.op == instr.op && isArithmeticOp(instr.op)) {
                uint64_t mergedArg = 0;
                if (!checkedAdd(last.arg, instr.arg, mergedArg)) {
                    optimized.push_back(instr);
                    return true;
                }
                last.arg = mergedArg;
                changed = true;
                if (last.arg == 0) {
                    optimized.pop_back();
                }
                return true;
            }

            if (last.op == OpCode::Set && instr.op == OpCode::Set) {
                last = instr;
                changed = true;
                return true;
            }

            if (last.op == OpCode::Zero && instr.op == OpCode::Zero) {
                changed = true;
                return true;
            }

            if (last.op == OpCode::Set && instr.op == OpCode::Zero) {
                last = instr;
                changed = true;
                return true;
            }

            if (last.op == OpCode::Set && instr.op == OpCode::Add) {
                uint64_t foldedValue = 0;
                if (!checkedAdd(last.arg, instr.arg, foldedValue)) {
                    optimized.push_back(instr);
                    return true;
                }
                last.arg = foldedValue;
                changed = true;
                if (last.arg == 0) {
                    last.op = OpCode::Zero;
                }
                return true;
            }

            if (last.op == OpCode::Set && instr.op == OpCode::Sub && last.arg >= instr.arg) {
                last.arg -= instr.arg;
                changed = true;
                if (last.arg == 0) {
                    last.op = OpCode::Zero;
                }
                return true;
            }

            if (last.op == OpCode::Zero && instr.op == OpCode::Set) {
                last = instr;
                changed = true;
                return true;
            }

            if (last.op == OpCode::Zero && instr.op == OpCode::Add) {
                last = Instruction{OpCode::Set, instr.arg, 0, instr.line, instr.column};
                changed = true;
                return true;
            }

            if (instr.op == OpCode::Goto &&
                (last.op == OpCode::Left || last.op == OpCode::Right)) {
                optimized.pop_back();
                changed = true;
            } else if (last.op == OpCode::Goto && instr.op == OpCode::Goto) {
                last = instr;
                changed = true;
                return true;
            }
        }

        optimized.push_back(instr);
        return true;
    }

    bool optimizeInstructions(std::vector<Instruction>& instructions) {
        bool changed = true;
        int passes = 0;
        const int MAX_PASSES = 50;

        while (changed && passes < MAX_PASSES) {
            rebuildRepeatLinks(instructions);

            changed = false;
            ++passes;

            std::vector<Instruction> optimized;
            optimized.reserve(instructions.size());

            for (size_t i = 0; i < instructions.size(); ++i) {
                const Instruction instr = instructions[i];

                if (instr.op == OpCode::RepeatStart) {
                    const size_t endIndex = static_cast<size_t>(instr.target);
                    const bool hasMatchingEnd = endIndex < instructions.size() &&
                                                instructions[endIndex].op == OpCode::RepeatEnd;

                    if (hasMatchingEnd) {
                        const size_t bodyStart = i + 1;
                        const size_t bodyEnd = endIndex;
                        const size_t bodySize = bodyEnd - bodyStart;

                        if (bodySize == 0) {
                            changed = true;
                            i = endIndex;
                            continue;
                        }

                        if (instr.arg == 0) {
                            changed = true;
                            i = endIndex;
                            continue;
                        }

                        if (instr.arg == 1) {
                            changed = true;
                            for (size_t j = bodyStart; j < bodyEnd; ++j) {
                                appendInstruction(optimized, instructions[j], changed);
                            }
                            i = endIndex;
                            continue;
                        }

                        if (bodySize == 1) {
                            Instruction body = instructions[bodyStart];

                            if (isFoldableRepeatOp(body.op)) {
                                bool canFold = true;
                                if (body.op == OpCode::Add || body.op == OpCode::Sub ||
                                    body.op == OpCode::Left || body.op == OpCode::Right) {
                                    uint64_t multipliedArg = 0;
                                    if (!checkedMultiply(body.arg, instr.arg, multipliedArg)) {
                                        canFold = false;
                                    } else {
                                        body.arg = multipliedArg;
                                    }
                                    if (canFold && body.arg == 0) {
                                        changed = true;
                                        i = endIndex;
                                        continue;
                                    }
                                }

                                if (canFold) {
                                    changed = true;
                                    appendInstruction(optimized, body, changed);
                                    i = endIndex;
                                    continue;
                                }
                            }
                        }
                    }
                }

                appendInstruction(optimized, instr, changed);
            }

            instructions = std::move(optimized);
        }

        rebuildRepeatLinks(instructions);
        return true;
    }

    bool parseStatements(std::vector<Instruction>& instructions, bool stopAtEnd, const Token& startToken = Token{}) {
        while (true) {
            bool statementWasRepeat = false;

            while (isSeparator(current().type)) {
                advance();
            }

            if (current().type == TokenType::EndKeyword) {
                if (stopAtEnd) {
                    advance();
                    return true;
                }
                setError(syntaxError("unexpected 'end'"));
                return false;
            }

            switch (current().type) {
                case TokenType::End:
                    if (stopAtEnd) {
                        std::string msg = "missing 'end' for repeat at line " + std::to_string(startToken.line) +
                                         ", column " + std::to_string(startToken.column);
                        setError(formatDiagnostic(source_, startToken.line, startToken.column, startToken.text.empty() ? 1 : startToken.text.length(), msg));
                        return false;
                    }
                    return true;

                case TokenType::Left: {
                    const Token command = current();
                    advance();
                    uint64_t count = 1;
                    if (current().type == TokenType::Number && !parseNumber(count, "left")) {
                        return false;
                    }
                    instructions.push_back(makeInstruction(OpCode::Left, count, 0, command));
                    break;
                }

                case TokenType::Right: {
                    const Token command = current();
                    advance();
                    uint64_t count = 1;
                    if (current().type == TokenType::Number && !parseNumber(count, "rght")) {
                        return false;
                    }
                    instructions.push_back(makeInstruction(OpCode::Right, count, 0, command));
                    break;
                }

                case TokenType::Add: {
                    const Token command = current();
                    advance();
                    uint64_t count = 1;
                    if (current().type == TokenType::Number && !parseNumber(count, "add")) {
                        return false;
                    }
                    instructions.push_back(makeInstruction(OpCode::Add, count, 0, command));
                    break;
                }

                case TokenType::Sub: {
                    const Token command = current();
                    advance();
                    uint64_t count = 1;
                    if (current().type == TokenType::Number && !parseNumber(count, "sub")) {
                        return false;
                    }
                    instructions.push_back(makeInstruction(OpCode::Sub, count, 0, command));
                    break;
                }

                case TokenType::Set: {
                    const Token command = current();
                    advance();
                    uint64_t value = 0;
                    if (!parseNumber(value, "set")) {
                        return false;
                    }
                    instructions.push_back(makeInstruction(OpCode::Set, value, 0, command));
                    break;
                }

                case TokenType::Goto: {
                    const Token command = current();
                    advance();
                    uint64_t target = 0;
                    if (!parseNumber(target, "goto")) {
                        return false;
                    }
                    instructions.push_back(makeInstruction(OpCode::Goto, target, 0, command));
                    break;
                }

                case TokenType::Output: {
                    const Token command = current();
                    advance();
                    instructions.push_back(makeInstruction(OpCode::Output, 0, 0, command));
                    break;
                }

                case TokenType::Read: {
                    const Token command = current();
                    advance();
                    instructions.push_back(makeInstruction(OpCode::Read, 0, 0, command));
                    break;
                }

                case TokenType::Print: {
                    const Token command = current();
                    advance();
                    instructions.push_back(makeInstruction(OpCode::Print, 0, 0, command));
                    break;
                }

                case TokenType::Zero: {
                    const Token command = current();
                    advance();
                    instructions.push_back(makeInstruction(OpCode::Zero, 0, 0, command));
                    break;
                }

                case TokenType::Repeat: {
                    const Token command = current();
                    advance();
                    uint64_t count = 0;
                    if (!parseNumber(count, "repeat")) {
                        return false;
                    }

                    statementWasRepeat = true;
                    const size_t startIndex = instructions.size();
                    instructions.push_back(makeInstruction(OpCode::RepeatStart, count, 0, command));

                    if (!parseStatements(instructions, true, command)) {
                        return false;
                    }

                    const size_t endIndex = instructions.size();
                    instructions[startIndex].target = endIndex;
                    instructions.push_back(Instruction{OpCode::RepeatEnd, 0, startIndex, command.line, command.column});
                    break;
                }

                case TokenType::Error:
                    setError(syntaxError("unexpected token '" + std::string(current().text) + "'"));
                    return false;

                default:
                    setError(syntaxError("unexpected token"));
                    return false;
            }

            if (current().type == TokenType::End) {
                if (stopAtEnd) {
                    std::string msg = "missing 'end' for repeat at line " + std::to_string(startToken.line) +
                                     ", column " + std::to_string(startToken.column);
                    setError(formatDiagnostic(source_, startToken.line, startToken.column, startToken.text.empty() ? 1 : startToken.text.length(), msg));
                    return false;
                }
                if (statementWasRepeat) {
                    return true;
                }
                setError(syntaxError("missing statement separator"));
                return false;
            }

            if (current().type == TokenType::EndKeyword) {
                setError(syntaxError("missing statement separator"));
                return false;
            }

            if (!isSeparator(current().type)) {
                setError(syntaxError("missing statement separator before " + tokenTextForError(current()) +
                                     "; end simple statements with ';' or a newline"));
                return false;
            }
        }
    }

public:
    ParseResult parse(std::string_view source) {
        source_ = source;
        tokens_.clear();
        pos_ = 0;
        error_.clear();

        Lexer lexer(source);
        Token tok;
        do {
            tok = lexer.next();
            if (tok.type == TokenType::Error) {
                return {{}, lexicalErrorFor(tok)};
            }
            tokens_.push_back(tok);
        } while (tok.type != TokenType::End);

        std::vector<Instruction> instructions;
        if (!parseStatements(instructions, false)) {
            return {{}, error_};
        }

        if (current().type == TokenType::EndKeyword) {
            return {{}, syntaxError("unexpected 'end'" )};
        }

        if (!optimizeInstructions(instructions)) {
            return {{}, "optimizer error"};
        }

        return {instructions, ""};
    }
};

ParseResult parseProgram(std::string_view source) {
    Parser parser;
    return parser.parse(source);
}

} // namespace yascript
