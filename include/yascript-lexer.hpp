#ifndef YASCRIPT_LEXER_HPP
#define YASCRIPT_LEXER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace yascript {

enum class TokenType : uint8_t {
    End,
    EndKeyword,
    Newline,
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
    Repeat,
    Semicolon,
    Number,
    Error,
};

struct Token {
    TokenType type;
    std::string_view text;
    uint32_t line;
    uint32_t column;
};

class Lexer {
public:
    explicit Lexer(std::string_view source);
    Token next();
    bool hasError() const { return errorMessage_ != ""; }
    const std::string& errorMessage() const { return errorMessage_; }

private:
    std::string_view source_;
    size_t pos_;
    uint32_t line_;
    uint32_t column_;
    std::string errorMessage_;

    void skipWhitespace();
    void skipComment();
    Token makeToken(TokenType type, size_t start, size_t end);
};

} // namespace yascript

#endif // YASCRIPT_LEXER_HPP
