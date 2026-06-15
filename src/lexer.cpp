#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

static const std::unordered_map<std::string, TokenType> keywords = {
    {"func",        TokenType::FUNC},
    {"return",      TokenType::RETURN},
    {"if",          TokenType::IF},
    {"else",        TokenType::ELSE},
    {"while",       TokenType::WHILE},
    {"for",         TokenType::FOR},
    {"import",      TokenType::IMPORT},
    {"struct",      TokenType::STRUCT},
    {"var",         TokenType::VAR},
    {"this",        TokenType::THIS},
    {"constructor", TokenType::CONSTRUCTOR},
    {"void",        TokenType::VOID},
    {"new",         TokenType::NEW},
    {"int",         TokenType::TYPE_INT},
    {"char",        TokenType::TYPE_CHAR},
    {"long",        TokenType::TYPE_LONG},
    {"float",       TokenType::TYPE_FLOAT},
    {"double",      TokenType::TYPE_DOUBLE},
};

Lexer::Lexer(const std::string& source)
    : source(source), pos(0), line(1), col(1) {}

char Lexer::current() const {
    if (pos >= source.size()) return '\0';
    return source[pos];
}

char Lexer::peek(int offset) const {
    if (pos + offset >= source.size()) return '\0';
    return source[pos + offset];
}

void Lexer::advance() {
    if (pos < source.size()) {
        if (source[pos] == '\n') { line++; col = 1; }
        else col++;
        pos++;
    }
}

void Lexer::skipWhitespaceAndComments() {
    while (pos < source.size()) {
        if (std::isspace(current())) {
            advance();
        } else if (current() == '/' && peek() == '/') {
            while (pos < source.size() && current() != '\n') advance();
        } else if (current() == '/' && peek() == '*') {
            advance(); advance();
            while (pos < source.size()) {
                if (current() == '*' && peek() == '/') {
                    advance(); advance();
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::readNumber() {
    int startLine = line, startCol = col;
    std::string val;
    bool isFloat = false;
    while (pos < source.size() && (std::isdigit(current()) || current() == '.')) {
        if (current() == '.') {
            if (isFloat) break;
            isFloat = true;
        }
        val += current();
        advance();
    }
    TokenType t = isFloat ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL;
    return Token{t, val, startLine, startCol};
}

Token Lexer::readString() {
    int startLine = line, startCol = col;
    advance(); // skip "
    std::string val;
    while (pos < source.size() && current() != '"') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n': val += '\n'; break;
                case 't': val += '\t'; break;
                case '\\': val += '\\'; break;
                case '"': val += '"'; break;
                default: val += current(); break;
            }
        } else {
            val += current();
        }
        advance();
    }
    advance(); // skip closing "
    return Token{TokenType::STRING_LITERAL, val, startLine, startCol};
}

Token Lexer::readIdentifierOrKeyword() {
    int startLine = line, startCol = col;
    std::string val;
    while (pos < source.size() && (std::isalnum(current()) || current() == '_')) {
        val += current();
        advance();
    }
    auto it = keywords.find(val);
    TokenType t = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;
    return Token{t, val, startLine, startCol};
}

Token Lexer::readOperatorOrDelimiter() {
    int startLine = line, startCol = col;
    char c = current();
    advance();
    switch (c) {
        case '+':
            if (current() == '=') { advance(); return {TokenType::PLUS_ASSIGN, "+=", startLine, startCol}; }
            return {TokenType::PLUS, "+", startLine, startCol};
        case '-':
            if (current() == '=') { advance(); return {TokenType::MINUS_ASSIGN, "-=", startLine, startCol}; }
            return {TokenType::MINUS, "-", startLine, startCol};
        case '*':
            if (current() == '=') { advance(); return {TokenType::STAR_ASSIGN, "*=", startLine, startCol}; }
            return {TokenType::STAR, "*", startLine, startCol};
        case '/':
            if (current() == '=') { advance(); return {TokenType::SLASH_ASSIGN, "/=", startLine, startCol}; }
            return {TokenType::SLASH, "/", startLine, startCol};
        case '%': return {TokenType::PERCENT, "%", startLine, startCol};
        case '&': return {TokenType::AMPERSAND, "&", startLine, startCol};
        case '=':
            if (current() == '=') { advance(); return {TokenType::EQ, "==", startLine, startCol}; }
            return {TokenType::ASSIGN, "=", startLine, startCol};
        case '!':
            if (current() == '=') { advance(); return {TokenType::NEQ, "!=", startLine, startCol}; }
            return {TokenType::UNKNOWN, "!", startLine, startCol};
        case '<':
            if (current() == '=') { advance(); return {TokenType::LE, "<=", startLine, startCol}; }
            return {TokenType::LT, "<", startLine, startCol};
        case '>':
            if (current() == '=') { advance(); return {TokenType::GE, ">=", startLine, startCol}; }
            return {TokenType::GT, ">", startLine, startCol};
        case '(': return {TokenType::LPAREN, "(", startLine, startCol};
        case ')': return {TokenType::RPAREN, ")", startLine, startCol};
        case '{': return {TokenType::LBRACE, "{", startLine, startCol};
        case '}': return {TokenType::RBRACE, "}", startLine, startCol};
        case '[': return {TokenType::LBRACKET, "[", startLine, startCol};
        case ']': return {TokenType::RBRACKET, "]", startLine, startCol};
        case ';': return {TokenType::SEMICOLON, ";", startLine, startCol};
        case ':': return {TokenType::COLON, ":", startLine, startCol};
        case ',': return {TokenType::COMMA, ",", startLine, startCol};
        case '.': return {TokenType::DOT, ".", startLine, startCol};
        default:  return {TokenType::UNKNOWN, std::string(1,c), startLine, startCol};
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespaceAndComments();
        if (pos >= source.size()) {
            tokens.push_back({TokenType::EOF_TOKEN, "", line, col});
            break;
        }
        char c = current();
        Token tok;
        if (std::isdigit(c)) {
            tok = readNumber();
        } else if (c == '"') {
            tok = readString();
        } else if (std::isalpha(c) || c == '_') {
            tok = readIdentifierOrKeyword();
        } else {
            tok = readOperatorOrDelimiter();
        }
        tokens.push_back(tok);
    }
    return tokens;
}
