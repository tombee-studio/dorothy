#pragma once
#include "token.h"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t pos;
    int line;
    int col;

    char current() const;
    char peek(int offset = 1) const;
    void advance();
    void skipWhitespaceAndComments();
    Token readNumber();
    Token readString();
    Token readIdentifierOrKeyword();
};
