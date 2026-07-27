#include "../include/yascript-runner.hpp"
#include "../include/yascript-diagnostics.hpp"

#include <limits>
#include <algorithm>
#include <sstream>

namespace yascript {

namespace {

std::string instructionLocation(const Instruction& instr, uint64_t pc) {
    std::ostringstream message;
    message << "instruction " << pc;
    if (instr.line != 0) {
        message << " at line " << instr.line << ", column " << instr.column;
    }
    return message.str();
}

size_t getOpKeywordLength(OpCode op) {
    switch (op) {
        case OpCode::Left: return 4;
        case OpCode::Right: return 4; // "rght"
        case OpCode::Add: return 3;
        case OpCode::Sub: return 3;
        case OpCode::Set: return 3;
        case OpCode::Goto: return 4;
        case OpCode::Output: return 6;
        case OpCode::Read: return 4;
        case OpCode::Print: return 5;
        case OpCode::Zero: return 4;
        case OpCode::RepeatStart: return 6;
        case OpCode::RepeatEnd: return 3;
    }
    return 1;
}

} // namespace

RunResult runProgram(const std::vector<Instruction>& program, std::istream& input, std::ostream& output, std::string_view source) {
    uint64_t initialTapeSize = 1;
    for (const auto& instr : program) {
        if (instr.op == OpCode::Goto) {
            initialTapeSize = std::max(initialTapeSize, instr.arg + 1);
        }
    }

    std::vector<uint64_t> tape(initialTapeSize, 0);
    uint64_t* tapePtr = tape.data();
    uint64_t pointer = 0;
    uint64_t pc = 0;
    std::vector<LoopFrame> repeatStack;
    std::string outputBuffer;
    outputBuffer.reserve(4096);
    std::string numStr;

    auto ensureTape = [&](uint64_t index) -> bool {
        if (index >= tape.size()) {
            if (index > std::numeric_limits<size_t>::max() - 1) {
                return false;
            }
            tape.resize(static_cast<size_t>(index + 1), 0);
            tapePtr = tape.data();
        }
        return true;
    };

    auto flushOutput = [&]() {
        if (!outputBuffer.empty()) {
            output << outputBuffer;
            outputBuffer.clear();
        }
    };

#if defined(__GNUC__) || defined(__clang__)
    // Threaded VM using GCC Labels-as-Values Extension
    static void* const dispatchTable[] = {
        &&handle_Left,
        &&handle_Right,
        &&handle_Add,
        &&handle_Sub,
        &&handle_Set,
        &&handle_Goto,
        &&handle_Output,
        &&handle_Read,
        &&handle_Print,
        &&handle_Zero,
        &&handle_RepeatStart,
        &&handle_RepeatEnd
    };

    #define DISPATCH() \
        if (pc >= program.size()) goto vm_end; \
        goto *dispatchTable[static_cast<uint8_t>(program[pc].op)]

    DISPATCH();

handle_Left: {
    const Instruction& instr = program[pc];
    if (pointer < instr.arg) {
        flushOutput();
        std::string msg = "pointer underflow at " + instructionLocation(instr, pc) +
                          ": left " + std::to_string(instr.arg) +
                          " from cell " + std::to_string(pointer);
        return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
    }
    pointer -= instr.arg;
    ++pc;
    DISPATCH();
}

handle_Right: {
    const Instruction& instr = program[pc];
    if (pointer > std::numeric_limits<uint64_t>::max() - instr.arg) {
        flushOutput();
        std::string msg = "pointer overflow at " + instructionLocation(instr, pc) +
                          ": rght " + std::to_string(instr.arg) +
                          " from cell " + std::to_string(pointer);
        return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
    }
    pointer += instr.arg;
    if (!ensureTape(pointer)) {
        flushOutput();
        std::string msg = "cannot grow tape to cell " + std::to_string(pointer) +
                          " at " + instructionLocation(instr, pc);
        return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
    }
    ++pc;
    DISPATCH();
}

handle_Add: {
    const Instruction& instr = program[pc];
    uint64_t& cell = tapePtr[pointer];
    if (cell > std::numeric_limits<uint64_t>::max() - instr.arg) {
        flushOutput();
        std::string msg = "value overflow at " + instructionLocation(instr, pc) +
                          ": add " + std::to_string(instr.arg) +
                          " to cell " + std::to_string(pointer) +
                          " containing " + std::to_string(cell);
        return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
    }
    cell += instr.arg;
    ++pc;
    DISPATCH();
}

handle_Sub: {
    const Instruction& instr = program[pc];
    uint64_t& cell = tapePtr[pointer];
    if (cell < instr.arg) {
        flushOutput();
        std::string msg = "value underflow at " + instructionLocation(instr, pc) +
                          ": sub " + std::to_string(instr.arg) +
                          " from cell " + std::to_string(pointer) +
                          " containing " + std::to_string(cell);
        return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
    }
    cell -= instr.arg;
    ++pc;
    DISPATCH();
}

handle_Set: {
    const Instruction& instr = program[pc];
    tapePtr[pointer] = instr.arg;
    ++pc;
    DISPATCH();
}

handle_Goto: {
    const Instruction& instr = program[pc];
    pointer = instr.arg;
    if (!ensureTape(pointer)) {
        flushOutput();
        std::string msg = "cannot grow tape to cell " + std::to_string(pointer) +
                          " at " + instructionLocation(instr, pc);
        return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
    }
    ++pc;
    DISPATCH();
}

handle_Output: {
    size_t start = static_cast<size_t>(pointer);
    size_t lastNonZero = start;
    for (size_t i = start; i < tape.size(); ++i) {
        if (tapePtr[i] != 0) {
            lastNonZero = i;
        }
    }
    for (size_t i = start; i <= lastNonZero; ++i) {
        outputBuffer += static_cast<char>(tapePtr[i] & 0xFF);
    }
    ++pc;
    DISPATCH();
}

handle_Print: {
    numStr = std::to_string(tapePtr[pointer]);
    outputBuffer += numStr;
    ++pc;
    DISPATCH();
}

handle_Read: {
    int ch = input.get();
    tapePtr[pointer] = (ch == EOF) ? 0 : static_cast<uint8_t>(ch);
    ++pc;
    DISPATCH();
}

handle_Zero: {
    tapePtr[pointer] = 0;
    ++pc;
    DISPATCH();
}

handle_RepeatStart: {
    const Instruction& instr = program[pc];
    if (instr.arg == 0) {
        pc = instr.target + 1;
    } else {
        repeatStack.push_back(LoopFrame{pc, instr.arg});
        ++pc;
    }
    DISPATCH();
}

handle_RepeatEnd: {
    const Instruction& instr = program[pc];
    if (repeatStack.empty()) {
        flushOutput();
        std::string msg = "internal repeat stack underflow at " + instructionLocation(instr, pc);
        return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
    }
    auto& frame = repeatStack.back();
    if (frame.remaining <= 1) {
        repeatStack.pop_back();
        ++pc;
    } else {
        --frame.remaining;
        pc = frame.start + 1;
    }
    DISPATCH();
}

vm_end:

#else
    // Fallback switch-based interpreter
    while (pc < program.size()) {
        const Instruction& instr = program[static_cast<size_t>(pc)];

        switch (instr.op) {
            case OpCode::Left: {
                if (pointer < instr.arg) {
                    flushOutput();
                    std::string msg = "pointer underflow at " + instructionLocation(instr, pc) +
                                      ": left " + std::to_string(instr.arg) +
                                      " from cell " + std::to_string(pointer);
                    return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
                }
                pointer -= instr.arg;
                ++pc;
                break;
            }

            case OpCode::Right: {
                if (pointer > std::numeric_limits<uint64_t>::max() - instr.arg) {
                    flushOutput();
                    std::string msg = "pointer overflow at " + instructionLocation(instr, pc) +
                                      ": rght " + std::to_string(instr.arg) +
                                      " from cell " + std::to_string(pointer);
                    return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
                }
                pointer += instr.arg;
                if (!ensureTape(pointer)) {
                    flushOutput();
                    std::string msg = "cannot grow tape to cell " + std::to_string(pointer) +
                                      " at " + instructionLocation(instr, pc);
                    return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
                }
                ++pc;
                break;
            }

            case OpCode::Add: {
                uint64_t& cell = tapePtr[pointer];
                if (cell > std::numeric_limits<uint64_t>::max() - instr.arg) {
                    flushOutput();
                    std::string msg = "value overflow at " + instructionLocation(instr, pc) +
                                      ": add " + std::to_string(instr.arg) +
                                      " to cell " + std::to_string(pointer) +
                                      " containing " + std::to_string(cell);
                    return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
                }
                cell += instr.arg;
                ++pc;
                break;
            }

            case OpCode::Sub: {
                uint64_t& cell = tapePtr[pointer];
                if (cell < instr.arg) {
                    flushOutput();
                    std::string msg = "value underflow at " + instructionLocation(instr, pc) +
                                      ": sub " + std::to_string(instr.arg) +
                                      " from cell " + std::to_string(pointer) +
                                      " containing " + std::to_string(cell);
                    return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
                }
                cell -= instr.arg;
                ++pc;
                break;
            }

            case OpCode::Set:
                tapePtr[pointer] = instr.arg;
                ++pc;
                break;

            case OpCode::Goto: {
                pointer = instr.arg;
                if (!ensureTape(pointer)) {
                    flushOutput();
                    std::string msg = "cannot grow tape to cell " + std::to_string(pointer) +
                                      " at " + instructionLocation(instr, pc);
                    return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
                }
                ++pc;
                break;
            }

            case OpCode::Output: {
                size_t start = static_cast<size_t>(pointer);
                size_t lastNonZero = start;
                for (size_t i = start; i < tape.size(); ++i) {
                    if (tapePtr[i] != 0) {
                        lastNonZero = i;
                    }
                }
                for (size_t i = start; i <= lastNonZero; ++i) {
                    outputBuffer += static_cast<char>(tapePtr[i] & 0xFF);
                }
                ++pc;
                break;
            }

            case OpCode::Print: {
                std::string numStr = std::to_string(tapePtr[pointer]);
                outputBuffer += numStr;
                ++pc;
                break;
            }

            case OpCode::Read: {
                int ch = input.get();
                tapePtr[pointer] = (ch == EOF) ? 0 : static_cast<uint8_t>(ch);
                ++pc;
                break;
            }

            case OpCode::Zero:
                tapePtr[pointer] = 0;
                ++pc;
                break;

            case OpCode::RepeatStart: {
                if (instr.arg == 0) {
                    pc = instr.target + 1;
                } else {
                    repeatStack.push_back(LoopFrame{pc, instr.arg});
                    ++pc;
                }
                break;
            }

            case OpCode::RepeatEnd: {
                if (repeatStack.empty()) {
                    flushOutput();
                    std::string msg = "internal repeat stack underflow at " + instructionLocation(instr, pc);
                    return {false, formatDiagnostic(source, instr.line, instr.column, getOpKeywordLength(instr.op), msg)};
                }
                auto& frame = repeatStack.back();
                if (frame.remaining <= 1) {
                    repeatStack.pop_back();
                    ++pc;
                } else {
                    --frame.remaining;
                    pc = frame.start + 1;
                }
                break;
            }
        }
    }
#endif

    flushOutput();
    return {true, ""};
}

} // namespace yascript
