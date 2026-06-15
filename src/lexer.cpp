#include "lexer.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& source)
    : source(source), pos(0), line(1), col(1) {}

char Lexer::current() const {
    if (pos >= source.size()) return '\0';
    return source[pos];
}

char Lexer::peek(int offset) const {
    size_t p = pos + offset;
    if (p >= source.size()) return '\0';
    return source[p];
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
    std::string num;
    bool isFloat = false;
    while (pos < source.size() && (std::isdigit(current()) || current() == '.')) {
        if (current() == '.') {
            if (isFloat) break;
            isFloat = true;
        }
        num += current();
        advance();
    }
    if (isFloat) {
        return {TOKEN_FLOAT_LITERAL, num, startLine, startCol};
    }
    return {TOKEN_INT_LITERAL, num, startLine, startCol};
}

Token Lexer::readString() {
    int startLine = line, startCol = col;
    advance(); // skip "
    std::string s;
    while (pos < source.size() && current() != '"') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case '\\': s += '\\'; break;
                case '"': s += '"'; break;
                default: s += current(); break;
            }
        } else {
            s += current();
        }
        advance();
    }
    if (pos < source.size()) advance(); // skip closing "
    return {TOKEN_STRING_LITERAL, s, startLine, startCol};
}

Token Lexer::readIdentifierOrKeyword() {
    int startLine = line, startCol = col;
    std::string id;
    while (pos < source.size() && (std::isalnum(current()) || current() == '_')) {
        id += current();
        advance();
    }
    // Keywords
    if (id == "func")        return {TOKEN_FUNC, id, startLine, startCol};
    if (id == "return")      return {TOKEN_RETURN, id, startLine, startCol};
    if (id == "if")          return {TOKEN_IF, id, startLine, startCol};
    if (id == "else")        return {TOKEN_ELSE, id, startLine, startCol};
    if (id == "while")       return {TOKEN_WHILE, id, startLine, startCol};
    if (id == "for")         return {TOKEN_FOR, id, startLine, startCol};
    if (id == "import")      return {TOKEN_IMPORT, id, startLine, startCol};
    if (id == "struct")      return {TOKEN_STRUCT, id, startLine, startCol};
    if (id == "var")         return {TOKEN_VAR, id, startLine, startCol};
    if (id == "constructor") return {TOKEN_CONSTRUCTOR, id, startLine, startCol};
    if (id == "this")        return {TOKEN_THIS, id, startLine, startCol};
    if (id == "void")        return {TOKEN_VOID, id, startLine, startCol};
    if (id == "int")         return {TOKEN_INT_TYPE, id, startLine, startCol};
    if (id == "char")        return {TOKEN_CHAR_TYPE, id, startLine, startCol};
    if (id == "long")        return {TOKEN_LONG_TYPE, id, startLine, startCol};
    if (id == "float")       return {TOKEN_FLOAT_TYPE, id, startLine, startCol};
    if (id == "double")      return {TOKEN_DOUBLE_TYPE, id, startLine, startCol};
    return {TOKEN_IDENTIFIER, id, startLine, startCol};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespaceAndComments();
        if (pos >= source.size()) {
            tokens.push_back({TOKEN_EOF, "", line, col});
            break;
        }
        int startLine = line, startCol = col;
        char c = current();

        if (std::isdigit(c)) {
            tokens.push_back(readNumber());
            continue;
        }
        if (c == '"') {
            tokens.push_back(readString());
            continue;
        }
        if (std::isalpha(c) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // Single/double char operators
        advance();
        switch (c) {
            case '+':
                if (current() == '=') { advance(); tokens.push_back({TOKEN_PLUS_ASSIGN, "+=", startLine, startCol}); }
                else tokens.push_back({TOKEN_PLUS, "+", startLine, startCol});
                break;
            case '-':
                if (current() == '=') { advance(); tokens.push_back({TOKEN_MINUS_ASSIGN, "-=", startLine, startCol}); }
                else tokens.push_back({TOKEN_MINUS, "-", startLine, startCol});
                break;
            case '*':
                if (current() == '=') { advance(); tokens.push_back({TOKEN_STAR_ASSIGN, "*=", startLine, startCol}); }
                else tokens.push_back({TOKEN_STAR, "*", startLine, startCol});
                break;
            case '/':
                if (current() == '=') { advance(); tokens.push_back({TOKEN_SLASH_ASSIGN, "/=", startLine, startCol}); }
                else tokens.push_back({TOKEN_SLASH, "/", startLine, startCol});
                break;
            case '%': tokens.push_back({TOKEN_PERCENT, "%", startLine, startCol}); break;
            case '&': tokens.push_back({TOKEN_AMPERSAND, "&", startLine, startCol}); break;
            case '=':
                if (current() == '=') { advance(); tokens.push_back({TOKEN_EQ, "==", startLine, startCol}); }
                else tokens.push_back({TOKEN_ASSIGN, "=", startLine, startCol});
                break;
            case '!':
                if (current() == '=') { advance(); tokens.push_back({TOKEN_NEQ, "!=", startLine, startCol}); }
                else throw std::runtime_error("Unexpected '!' at line " + std::to_string(startLine));
                break;
            case '<':
                if (current() == '=') { advance(); tokens.push_back({TOKEN_LE, "<=", startLine, startCol}); }
                else tokens.push_back({TOKEN_LT, "<", startLine, startCol});
                break;
            case '>':
                if (current() == '=') { advance(); tokens.push_back({TOKEN_GE, ">=", startLine, startCol}); }
                else tokens.push_back({TOKEN_GT, ">", startLine, startCol});
                break;
            case '(': tokens.push_back({TOKEN_LPAREN, "(", startLine, startCol}); break;
            case ')': tokens.push_back({TOKEN_RPAREN, ")", startLine, startCol}); break;
            case '{': tokens.push_back({TOKEN_LBRACE, "{", startLine, startCol}); break;
            case '}': tokens.push_back({TOKEN_RBRACE, "}", startLine, startCol}); break;
            case '[': tokens.push_back({TOKEN_LBRACKET, "[", startLine, startCol}); break;
            case ']': tokens.push_back({TOKEN_RBRACKET, "]", startLine, startCol}); break;
            case ';': tokens.push_back({TOKEN_SEMICOLON, ";", startLine, startCol}); break;
            case ':': tokens.push_back({TOKEN_COLON, ":", startLine, startCol}); break;
            case ',': tokens.push_back({TOKEN_COMMA, ",", startLine, startCol}); break;
            case '.': tokens.push_back({TOKEN_DOT, ".", startLine, startCol}); break;
            default:
                throw std::runtime_error(std::string("Unknown character '") + c + "' at line " + std::to_string(startLine));
        }
    }
    return tokens;
}
