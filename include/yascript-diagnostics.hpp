#ifndef YASCRIPT_DIAGNOSTICS_HPP
#define YASCRIPT_DIAGNOSTICS_HPP

#include <string>
#include <string_view>
#include <cstdint>

namespace yascript {

inline std::string formatDiagnostic(std::string_view source, uint32_t line, uint32_t column, size_t length, const std::string& message) {
    if (source.empty() || line == 0 || column == 0) {
        return message;
    }

    uint32_t currentLine = 1;
    size_t lineStart = 0;
    size_t lineEnd = std::string_view::npos;

    for (size_t i = 0; i < source.size(); ++i) {
        if (currentLine == line) {
            lineStart = i;
            while (i < source.size() && source[i] != '\n') {
                ++i;
            }
            lineEnd = i;
            break;
        }
        if (source[i] == '\n') {
            ++currentLine;
        }
    }

    if (currentLine == line && lineEnd == std::string_view::npos) {
        lineEnd = source.size();
    }

    if (lineStart >= source.size() || lineEnd == std::string_view::npos || lineStart > lineEnd) {
        return message;
    }

    std::string_view lineContent = source.substr(lineStart, lineEnd - lineStart);
    if (!lineContent.empty() && lineContent.back() == '\r') {
        lineContent.remove_suffix(1);
    }

    std::string lineNumStr = std::to_string(line);
    size_t lineNumWidth = lineNumStr.length();

    std::string result = message + "\n";
    result += " " + lineNumStr + " | " + std::string(lineContent) + "\n";
    result += std::string(lineNumWidth + 2, ' ') + "| ";

    uint32_t col = 1;
    for (size_t i = 0; i < lineContent.size() && col < column; ++i) {
        if (lineContent[i] == '\t') {
            result += "    ";
            col += 4;
        } else {
            result += " ";
            col += 1;
        }
    }

    size_t caretCount = (length > 0) ? length : 1;
    result += std::string(caretCount, '^');

    return result;
}

} // namespace yascript

#endif // YASCRIPT_DIAGNOSTICS_HPP
