#include "../include/yascript-lexer.hpp"

#include <cctype>

namespace yascript {

Lexer::Lexer(std::string_view source)
    : source_(source), pos_(0), line_(1), column_(1) {}

void Lexer::skipWhitespace() {
    while (pos_ < source_.size()) {
        const unsigned char ch = static_cast<unsigned char>(source_[pos_]);
        if (ch == '\n') {
            break;
        }
        if (!std::isspace(ch)) {
            break;
        }
        ++column_;
        ++pos_;
    }
}

void Lexer::skipComment() {
    if (source_[pos_] == '#' ||
        (pos_ + 1 < source_.size() && source_[pos_] == '/' && source_[pos_ + 1] == '/')) {
        while (pos_ < source_.size() && source_[pos_] != '\n') {
            ++pos_;
            ++column_;
        }
    }
}

Token Lexer::makeToken(TokenType type, size_t start, size_t end) {
    return Token{type, source_.substr(start, end - start), line_, column_ - (uint32_t)(end - start)};
}

Token Lexer::next() {
    while (true) {
        skipWhitespace();
        if (pos_ >= source_.size()) {
            return Token{TokenType::End, "", line_, column_};
        }
        if (source_[pos_] == '\n') {
            const uint32_t tokenLine = line_;
            const uint32_t tokenColumn = column_;
            ++pos_;
            ++line_;
            column_ = 1;
            return Token{TokenType::Newline, "\n", tokenLine, tokenColumn};
        }
        if (source_[pos_] == '#' || (pos_ + 1 < source_.size() && source_[pos_] == '/' && source_[pos_ + 1] == '/')) {
            skipComment();
            continue;
        }
        break;
    }

    uint32_t startCol = column_;
    size_t start = pos_;

    if (source_[pos_] == ';') {
        ++pos_;
        ++column_;
        return Token{TokenType::Semicolon, ";", line_, startCol};
    }

    if (std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
        while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
            ++pos_;
            ++column_;
        }
        return makeToken(TokenType::Number, start, pos_);
    }

    if (std::isalpha(static_cast<unsigned char>(source_[pos_])) || source_[pos_] == '_') {
        while (pos_ < source_.size() &&
               (std::isalnum(static_cast<unsigned char>(source_[pos_])) || source_[pos_] == '_')) {
            ++pos_;
            ++column_;
        }
        std::string_view word = source_.substr(start, pos_ - start);

        if (word == "left") return makeToken(TokenType::Left, start, pos_);
        if (word == "rght") return makeToken(TokenType::Right, start, pos_);
        if (word == "add") return makeToken(TokenType::Add, start, pos_);
        if (word == "sub") return makeToken(TokenType::Sub, start, pos_);
        if (word == "set") return makeToken(TokenType::Set, start, pos_);
        if (word == "goto") return makeToken(TokenType::Goto, start, pos_);
        if (word == "output") return makeToken(TokenType::Output, start, pos_);
        if (word == "read") return makeToken(TokenType::Read, start, pos_);
        if (word == "print") return makeToken(TokenType::Print, start, pos_);
        if (word == "zero") return makeToken(TokenType::Zero, start, pos_);
        if (word == "repeat") return makeToken(TokenType::Repeat, start, pos_);
        if (word == "end") return makeToken(TokenType::EndKeyword, start, pos_);

        return Token{TokenType::Error, word, line_, startCol};
    }

    ++pos_;
    ++column_;
    return Token{TokenType::Error, source_.substr(start, 1), line_, startCol};
}

} // namespace yascript
