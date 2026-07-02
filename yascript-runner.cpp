#include "yascript-runner.hpp"

#include <limits>

namespace yascript {

RunResult runProgram(const std::vector<Instruction> &program, std::istream &input, std::ostream &output) {
    std::vector<unsigned long long> tape(1, 0);
    unsigned long long pointer = 0;

    auto ensureTape = [&](unsigned long long index) {
        if (index >= tape.size()) {
            if (index > std::numeric_limits<size_t>::max() - 1) {
                return false;
            }
            tape.resize(static_cast<size_t>(index + 1), 0);
        }
        return true;
    };

    std::vector<std::pair<unsigned long long, unsigned long long>> loopStack;
    unsigned long long pc = 0;

    while (pc < program.size()) {
        const Instruction &instr = program[static_cast<size_t>(pc)];
        switch (instr.op) {
            case OpCode::Left:
                if (pointer == 0) {
                    return {false, "pointer underflow: already at position 0"};
                }
                --pointer;
                ++pc;
                break;

            case OpCode::Right:
                ++pointer;
                if (!ensureTape(pointer)) {
                    return {false, "memory error: cannot extend tape"};
                }
                ++pc;
                break;

            case OpCode::Add:
                tape[static_cast<size_t>(pointer)]++;
                ++pc;
                break;

            case OpCode::Min:
                if (tape[static_cast<size_t>(pointer)] == 0) {
                    return {false, "value underflow: cell cannot go below 0"};
                }
                --tape[static_cast<size_t>(pointer)];
                ++pc;
                break;

            case OpCode::Set:
                tape[static_cast<size_t>(pointer)] = instr.arg;
                ++pc;
                break;

            case OpCode::Goto:
                pointer = instr.arg;
                if (!ensureTape(pointer)) {
                    return {false, "memory error: cannot extend tape"};
                }
                ++pc;
                break;

            case OpCode::Output: {
                size_t start = static_cast<size_t>(pointer);
                size_t lastNonZero = start;
                for (size_t index = start; index < tape.size(); ++index) {
                    if (tape[index] != 0) {
                        lastNonZero = index;
                    }
                }
                for (size_t index = start; index <= lastNonZero; ++index) {
                    unsigned char ch = static_cast<unsigned char>(tape[index] & 0xFF);
                    output << ch;
                }
                ++pc;
                break;
            }

            case OpCode::Print: {
                unsigned char ch = static_cast<unsigned char>(tape[static_cast<size_t>(pointer)] & 0xFF);
                output << ch;
                ++pc;
                break;
            }

            case OpCode::Read: {
                int ch = input.get();
                if (ch == EOF) {
                    tape[static_cast<size_t>(pointer)] = 0;
                } else {
                    tape[static_cast<size_t>(pointer)] = static_cast<unsigned char>(ch);
                }
                ++pc;
                break;
            }

            case OpCode::Zero:
                tape[static_cast<size_t>(pointer)] = 0;
                ++pc;
                break;

            case OpCode::LoopStart:
                if (instr.arg == 0ULL) {
                    pc = instr.target + 1;
                } else {
                    loopStack.emplace_back(pc, instr.arg);
                    ++pc;
                }
                break;

            case OpCode::LoopEnd: {
                if (loopStack.empty()) {
                    return {false, "loop stack underflow"};
                }
                auto &state = loopStack.back();
                if (state.second <= 1ULL) {
                    loopStack.pop_back();
                    ++pc;
                } else {
                    state.second -= 1ULL;
                    pc = state.first + 1ULL;
                }
                break;
            }
        }
    }

    return {true, {}};
}

} // namespace yascript
