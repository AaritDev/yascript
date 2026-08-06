#include "../include/yascript-parser.hpp"
#include "../include/yascript-lexer.hpp"
#include "../include/yascript-diagnostics.hpp"

#include <charconv>
#include <limits>
#include <unordered_map>

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
    struct Variable {
        uint64_t cellIndex;
        bool isConstant;
        Token token;
    };

    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::string error_;
    std::string_view source_;

    std::unordered_map<std::string, Variable> symbolTable_;
    uint64_t nextFreeCell_ = 0;
    uint64_t tempCounter_ = 0;

    uint64_t allocateTemp() {
        return nextFreeCell_ + tempCounter_++;
    }

    void freeTemp() {
        if (tempCounter_ > 0) {
            tempCounter_--;
        }
    }

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
        return Instruction{arg, target, token.line, token.column, op};
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
                last = Instruction{instr.arg, 0, instr.line, instr.column, OpCode::Set};
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
            std::vector<size_t> oldToNew(instructions.size(), 0);

            for (size_t i = 0; i < instructions.size(); ++i) {
                oldToNew[i] = optimized.size();
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

            // Update all targets using oldToNew map
            for (auto& instr : optimized) {
                if (instr.op == OpCode::RepeatStart || instr.op == OpCode::RepeatEnd ||
                    instr.op == OpCode::Jump || instr.op == OpCode::JumpIfFalse) {
                    if (instr.target < oldToNew.size()) {
                        instr.target = oldToNew[instr.target];
                    } else {
                        instr.target = optimized.size();
                    }
                }
            }

            instructions = std::move(optimized);
        }

        rebuildRepeatLinks(instructions);
        return true;
    }

    uint64_t parseExpression(std::vector<Instruction>& instructions) {
        return parseAssignment(instructions);
    }

    uint64_t parseAssignment(std::vector<Instruction>& instructions) {
        if (current().type == TokenType::Identifier && (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::Equal)) {
            Token varToken = current();
            advance(); // identifier
            advance(); // '='
            
            uint64_t valCell = parseAssignment(instructions);
            if (valCell == std::numeric_limits<uint64_t>::max()) return valCell;
            
            std::string varName(varToken.text);
            auto it = symbolTable_.find(varName);
            if (it == symbolTable_.end()) {
                setError(syntaxError("undefined variable '" + varName + "'", varToken));
                return std::numeric_limits<uint64_t>::max();
            }
            if (it->second.isConstant) {
                setError(syntaxError("cannot reassign constant '" + varName + "'", varToken));
                return std::numeric_limits<uint64_t>::max();
            }
            
            instructions.push_back(makeInstruction(OpCode::Copy, valCell, it->second.cellIndex, varToken));
            if (valCell >= nextFreeCell_) {
                freeTemp();
            }
            return it->second.cellIndex;
        }
        return parseComparison(instructions);
    }

    uint64_t parseComparison(std::vector<Instruction>& instructions) {
        uint64_t lhs = parseTerm(instructions);
        if (lhs == std::numeric_limits<uint64_t>::max()) return lhs;
        
        while (current().type == TokenType::EqualEqual || current().type == TokenType::BangEqual ||
               current().type == TokenType::Less || current().type == TokenType::LessEqual ||
               current().type == TokenType::Greater || current().type == TokenType::GreaterEqual) {
            Token opTok = current();
            advance();
            
            uint64_t rhs = parseTerm(instructions);
            if (rhs == std::numeric_limits<uint64_t>::max()) return rhs;
            
            if (lhs < nextFreeCell_) {
                uint64_t temp = allocateTemp();
                instructions.push_back(makeInstruction(OpCode::Copy, lhs, temp, opTok));
                lhs = temp;
            }
            
            OpCode op;
            if (opTok.type == TokenType::EqualEqual) op = OpCode::CompareEqual;
            else if (opTok.type == TokenType::BangEqual) op = OpCode::CompareNotEqual;
            else if (opTok.type == TokenType::Less) op = OpCode::CompareLess;
            else if (opTok.type == TokenType::LessEqual) op = OpCode::CompareLessEqual;
            else if (opTok.type == TokenType::Greater) op = OpCode::CompareGreater;
            else op = OpCode::CompareGreaterEqual;
            
            instructions.push_back(makeInstruction(op, rhs, lhs, opTok));
            if (rhs >= nextFreeCell_) {
                freeTemp();
            }
        }
        return lhs;
    }

    uint64_t parseTerm(std::vector<Instruction>& instructions) {
        uint64_t lhs = parseFactor(instructions);
        if (lhs == std::numeric_limits<uint64_t>::max()) return lhs;
        
        while (current().type == TokenType::Plus || current().type == TokenType::Minus) {
            Token opTok = current();
            advance();
            
            uint64_t rhs = parseFactor(instructions);
            if (rhs == std::numeric_limits<uint64_t>::max()) return rhs;
            
            if (lhs < nextFreeCell_) {
                uint64_t temp = allocateTemp();
                instructions.push_back(makeInstruction(OpCode::Copy, lhs, temp, opTok));
                lhs = temp;
            }
            
            OpCode op = (opTok.type == TokenType::Plus) ? OpCode::AddVar : OpCode::SubVar;
            instructions.push_back(makeInstruction(op, rhs, lhs, opTok));
            if (rhs >= nextFreeCell_) {
                freeTemp();
            }
        }
        return lhs;
    }

    uint64_t parseFactor(std::vector<Instruction>& instructions) {
        uint64_t lhs = parsePrimary(instructions);
        if (lhs == std::numeric_limits<uint64_t>::max()) return lhs;
        
        while (current().type == TokenType::Star || current().type == TokenType::Slash || current().type == TokenType::Percent) {
            Token opTok = current();
            advance();
            
            uint64_t rhs = parsePrimary(instructions);
            if (rhs == std::numeric_limits<uint64_t>::max()) return rhs;
            
            if (lhs < nextFreeCell_) {
                uint64_t temp = allocateTemp();
                instructions.push_back(makeInstruction(OpCode::Copy, lhs, temp, opTok));
                lhs = temp;
            }
            
            OpCode op;
            if (opTok.type == TokenType::Star) op = OpCode::MulVar;
            else if (opTok.type == TokenType::Slash) op = OpCode::DivVar;
            else op = OpCode::ModVar;
            
            instructions.push_back(makeInstruction(op, rhs, lhs, opTok));
            if (rhs >= nextFreeCell_) {
                freeTemp();
            }
        }
        return lhs;
    }

    uint64_t parsePrimary(std::vector<Instruction>& instructions) {
        const Token tok = current();
        if (tok.type == TokenType::Number) {
            uint64_t val = 0;
            if (!parseNumber(val, "primary")) {
                return std::numeric_limits<uint64_t>::max();
            }
            uint64_t temp = allocateTemp();
            instructions.push_back(makeInstruction(OpCode::Goto, temp, 0, tok));
            instructions.push_back(makeInstruction(OpCode::Set, val, 0, tok));
            return temp;
        }
        
        if (tok.type == TokenType::Identifier) {
            std::string name(tok.text);
            advance();
            auto it = symbolTable_.find(name);
            if (it == symbolTable_.end()) {
                setError(syntaxError("undefined variable '" + name + "'", tok));
                return std::numeric_limits<uint64_t>::max();
            }
            return it->second.cellIndex;
        }
        
        if (tok.type == TokenType::OpenParen) {
            advance(); // '('
            uint64_t cell = parseExpression(instructions);
            if (cell == std::numeric_limits<uint64_t>::max()) return cell;
            
            if (current().type != TokenType::CloseParen) {
                setError(syntaxError("expected ')' after expression, got " + tokenTextForError(current())));
                return std::numeric_limits<uint64_t>::max();
            }
            advance(); // ')'
            return cell;
        }
        
        setError(syntaxError("expected expression, got " + tokenTextForError(tok)));
        return std::numeric_limits<uint64_t>::max();
    }

    bool parseStatements(std::vector<Instruction>& instructions, bool stopAtEnd, bool stopAtElse = false, const Token& startToken = Token{}) {
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

            if (current().type == TokenType::Else) {
                if (stopAtElse) {
                    return true;
                }
                setError(syntaxError("unexpected 'else'"));
                return false;
            }

            switch (current().type) {
                case TokenType::End:
                    if (stopAtEnd) {
                        std::string msg = "missing 'end' for block starting at line " + std::to_string(startToken.line) +
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

                    if (!parseStatements(instructions, true, false, command)) {
                        return false;
                    }

                    const size_t endIndex = instructions.size();
                    instructions[startIndex].target = endIndex;
                    instructions.push_back(Instruction{0, startIndex, command.line, command.column, OpCode::RepeatEnd});
                    break;
                }

                case TokenType::Let:
                case TokenType::Const: {
                    bool isConst = (current().type == TokenType::Const);
                    Token declTok = current();
                    advance(); // let/const
                    
                    if (current().type != TokenType::Identifier) {
                        setError(syntaxError("expected identifier after let/const", current()));
                        return false;
                    }
                    Token varTok = current();
                    std::string varName(varTok.text);
                    advance();
                    
                    if (symbolTable_.find(varName) != symbolTable_.end()) {
                        setError(syntaxError("variable '" + varName + "' is already declared", varTok));
                        return false;
                    }
                    
                    uint64_t varCell = nextFreeCell_++;
                    symbolTable_[varName] = Variable{varCell, isConst, varTok};
                    
                    if (current().type == TokenType::Equal) {
                        advance(); // '='
                        uint64_t valCell = parseExpression(instructions);
                        if (valCell == std::numeric_limits<uint64_t>::max()) return false;
                        
                        instructions.push_back(makeInstruction(OpCode::Copy, valCell, varCell, declTok));
                        if (valCell >= nextFreeCell_) {
                            freeTemp();
                        }
                    } else {
                        if (isConst) {
                            setError(syntaxError("const declaration requires an initializer", varTok));
                            return false;
                        }
                        instructions.push_back(makeInstruction(OpCode::Goto, varCell, 0, declTok));
                        instructions.push_back(makeInstruction(OpCode::Zero, 0, 0, declTok));
                    }
                    statementWasRepeat = true;
                    break;
                }

                case TokenType::Identifier: {
                    if (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::Equal) {
                        uint64_t valCell = parseAssignment(instructions);
                        if (valCell == std::numeric_limits<uint64_t>::max()) return false;
                        statementWasRepeat = true;
                    } else {
                        setError(syntaxError("unexpected identifier '" + std::string(current().text) + "'"));
                        return false;
                    }
                    break;
                }

                case TokenType::If: {
                    Token ifTok = current();
                    advance(); // if
                    
                    uint64_t condCell = parseExpression(instructions);
                    if (condCell == std::numeric_limits<uint64_t>::max()) return false;
                    
                    size_t jumpIfFalseIdx = instructions.size();
                    instructions.push_back(makeInstruction(OpCode::JumpIfFalse, condCell, 0, ifTok));
                    if (condCell >= nextFreeCell_) {
                        freeTemp();
                    }
                    
                    if (!parseStatements(instructions, true, true, ifTok)) {
                        return false;
                    }
                    
                    if (current().type == TokenType::Else) {
                        advance(); // consume 'else'
                        
                        size_t jumpIdx = instructions.size();
                        instructions.push_back(makeInstruction(OpCode::Jump, 0, 0, ifTok));
                        
                        instructions[jumpIfFalseIdx].target = instructions.size();
                        
                        if (!parseStatements(instructions, true, false, ifTok)) {
                            return false;
                        }
                        
                        instructions[jumpIdx].target = instructions.size();
                    } else {
                        instructions[jumpIfFalseIdx].target = instructions.size();
                    }
                    statementWasRepeat = true;
                    break;
                }

                case TokenType::While: {
                    Token whileTok = current();
                    advance(); // while
                    
                    size_t startIdx = instructions.size();
                    
                    uint64_t condCell = parseExpression(instructions);
                    if (condCell == std::numeric_limits<uint64_t>::max()) return false;
                    
                    size_t jumpIfFalseIdx = instructions.size();
                    instructions.push_back(makeInstruction(OpCode::JumpIfFalse, condCell, 0, whileTok));
                    if (condCell >= nextFreeCell_) {
                        freeTemp();
                    }
                    
                    if (!parseStatements(instructions, true, false, whileTok)) {
                        return false;
                    }
                    
                    instructions.push_back(makeInstruction(OpCode::Jump, 0, startIdx, whileTok));
                    instructions[jumpIfFalseIdx].target = instructions.size();
                    statementWasRepeat = true;
                    break;
                }

                case TokenType::Print: {
                    const Token command = current();
                    advance();
                    if (isSeparator(current().type) || current().type == TokenType::End ||
                        current().type == TokenType::EndKeyword || current().type == TokenType::Else) {
                        instructions.push_back(makeInstruction(OpCode::Print, 0, 0, command));
                    } else {
                        uint64_t valCell = parseExpression(instructions);
                        if (valCell == std::numeric_limits<uint64_t>::max()) return false;
                        instructions.push_back(makeInstruction(OpCode::PrintVar, valCell, 0, command));
                        if (valCell >= nextFreeCell_) {
                            freeTemp();
                        }
                        statementWasRepeat = true;
                    }
                    break;
                }

                case TokenType::Output: {
                    const Token command = current();
                    advance();
                    if (isSeparator(current().type) || current().type == TokenType::End ||
                        current().type == TokenType::EndKeyword || current().type == TokenType::Else) {
                        instructions.push_back(makeInstruction(OpCode::Output, 0, 0, command));
                    } else {
                        uint64_t valCell = parseExpression(instructions);
                        if (valCell == std::numeric_limits<uint64_t>::max()) return false;
                        instructions.push_back(makeInstruction(OpCode::OutputVar, valCell, 0, command));
                        if (valCell >= nextFreeCell_) {
                            freeTemp();
                        }
                        statementWasRepeat = true;
                    }
                    break;
                }

                case TokenType::Read: {
                    const Token command = current();
                    advance();
                    if (isSeparator(current().type) || current().type == TokenType::End ||
                        current().type == TokenType::EndKeyword || current().type == TokenType::Else) {
                        instructions.push_back(makeInstruction(OpCode::Read, 0, 0, command));
                    } else {
                        if (current().type != TokenType::Identifier) {
                            setError(syntaxError("expected identifier after read", current()));
                            return false;
                        }
                        Token varTok = current();
                        std::string varName(varTok.text);
                        advance();
                        
                        auto it = symbolTable_.find(varName);
                        if (it == symbolTable_.end()) {
                            setError(syntaxError("undefined variable '" + varName + "'", varTok));
                            return false;
                        }
                        if (it->second.isConstant) {
                            setError(syntaxError("cannot read into constant '" + varName + "'", varTok));
                            return false;
                        }
                        instructions.push_back(makeInstruction(OpCode::ReadVar, it->second.cellIndex, 0, command));
                        statementWasRepeat = true;
                    }
                    break;
                }

                case TokenType::Error:
                    setError(syntaxError("unexpected token '" + std::string(current().text) + "'"));
                    return false;

                default:
                    setError(syntaxError("unexpected token"));
                    return false;
            }

            // Block statements (if, while, let, const, repeat, assignment) don't require a trailing separator
            if (statementWasRepeat) {
                continue;
            }

            // Simple statements always require a separator before the next token
            if (current().type == TokenType::End) {
                if (stopAtEnd) {
                    std::string msg = "missing 'end' for block starting at line " + std::to_string(startToken.line) +
                                     ", column " + std::to_string(startToken.column);
                    setError(formatDiagnostic(source_, startToken.line, startToken.column, startToken.text.empty() ? 1 : startToken.text.length(), msg));
                    return false;
                }
                return true;
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
        symbolTable_.clear();
        nextFreeCell_ = 0;
        tempCounter_ = 0;

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
