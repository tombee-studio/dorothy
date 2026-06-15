#pragma once
#include "token.h"
#include <vector>
#include <string>

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();
    
private:
    std::string source;
    size_t pos;
    int line;
    
    char current();
    char peek(int offset = 1);
    void advance();
    void skipWhitespaceAndComments();
    Token readNumber();
    Token readString();
    Token readIdentifierOrKeyword();
};
