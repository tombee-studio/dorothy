#include "lexer.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& source) : source(source), pos(0), line(1) {}

char Lexer::current() {
    if (pos >= source.size()) return '\0';
    return source[pos];
}

char Lexer::peek(int offset) {
    size_t p = pos + offset;
    if (p >= source.size()) return '\0';
    return source[p];
}

void Lexer::advance() {
    if (pos < source.size()) {
        if (source[pos] == '\n') line++;
        pos++;
    }
}

void Lexer::skipWhitespaceAndComments() {
    while (pos < source.size()) {
        char c = current();
        if (std::isspace(c)) {
            advance();
        } else if (c == '/' && peek() == '/') {
            // Line comment
            while (pos < source.size() && current() != '\n') advance();
        } else if (c == '/' && peek() == '*') {
            // Block comment
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
    std::string num;
    bool is_float = false;
    while (pos < source.size() && (std::isdigit(current()) || current() == '.')) {
        if (current() == '.') {
            if (is_float) break;
            is_float = true;
        }
        num += current();
        advance();
    }
    if (is_float) {
        return Token(TokenType::FLOAT_LITERAL, num, line);
    }
    return Token(TokenType::INTEGER, num, line);
}

Token Lexer::readString() {
    advance(); // skip opening quote
    std::string str;
    while (pos < source.size() && current() != '"') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n': str += '\n'; break;
                case 't': str += '\t'; break;
                case '\\': str += '\\'; break;
                case '"': str += '"'; break;
                default: str += current(); break;
            }
        } else {
            str += current();
        }
        advance();
    }
    if (pos < source.size()) advance(); // skip closing quote
    return Token(TokenType::STRING_LITERAL, str, line);
}

Token Lexer::readIdentifierOrKeyword() {
    std::string ident;
    while (pos < source.size() && (std::isalnum(current()) || current() == '_')) {
        ident += current();
        advance();
    }
    
    // Keywords
    if (ident == "func") return Token(TokenType::FUNC, ident, line);
    if (ident == "return") return Token(TokenType::RETURN, ident, line);
    if (ident == "if") return Token(TokenType::IF, ident, line);
    if (ident == "else") return Token(TokenType::ELSE, ident, line);
    if (ident == "while") return Token(TokenType::WHILE, ident, line);
    if (ident == "for") return Token(TokenType::FOR, ident, line);
    if (ident == "import") return Token(TokenType::IMPORT, ident, line);
    if (ident == "struct") return Token(TokenType::STRUCT, ident, line);
    if (ident == "var") return Token(TokenType::VAR, ident, line);
    if (ident == "constructor") return Token(TokenType::CONSTRUCTOR, ident, line);
    if (ident == "this") return Token(TokenType::THIS, ident, line);
    if (ident == "void") return Token(TokenType::VOID, ident, line);
    if (ident == "int") return Token(TokenType::TYPE_INT, ident, line);
    if (ident == "char") return Token(TokenType::TYPE_CHAR, ident, line);
    if (ident == "long") return Token(TokenType::TYPE_LONG, ident, line);
    if (ident == "float") return Token(TokenType::TYPE_FLOAT, ident, line);
    if (ident == "double") return Token(TokenType::TYPE_DOUBLE, ident, line);
    
    return Token(TokenType::IDENTIFIER, ident, line);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (true) {
        skipWhitespaceAndComments();
        if (pos >= source.size()) {
            tokens.push_back(Token(TokenType::END_OF_FILE, "", line));
            break;
        }
        
        char c = current();
        
        if (std::isdigit(c)) {
            tokens.push_back(readNumber());
        } else if (c == '"') {
            tokens.push_back(readString());
        } else if (std::isalpha(c) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
        } else {
            switch (c) {
                case '+':
                    advance();
                    if (current() == '=') { advance(); tokens.push_back(Token(TokenType::PLUS_ASSIGN, "+=", line)); }
                    else tokens.push_back(Token(TokenType::PLUS, "+", line));
                    break;
                case '-':
                    advance();
                    if (current() == '=') { advance(); tokens.push_back(Token(TokenType::MINUS_ASSIGN, "-=", line)); }
                    else tokens.push_back(Token(TokenType::MINUS, "-", line));
                    break;
                case '*':
                    advance();
                    if (current() == '=') { advance(); tokens.push_back(Token(TokenType::STAR_ASSIGN, "*=", line)); }
                    else tokens.push_back(Token(TokenType::STAR, "*", line));
                    break;
                case '/':
                    advance();
                    if (current() == '=') { advance(); tokens.push_back(Token(TokenType::SLASH_ASSIGN, "/=", line)); }
                    else tokens.push_back(Token(TokenType::SLASH, "/", line));
                    break;
                case '%':
                    advance();
                    tokens.push_back(Token(TokenType::PERCENT, "%", line));
                    break;
                case '=':
                    advance();
                    if (current() == '=') { advance(); tokens.push_back(Token(TokenType::EQ, "==", line)); }
                    else tokens.push_back(Token(TokenType::ASSIGN, "=", line));
                    break;
                case '!':
                    advance();
                    if (current() == '=') { advance(); tokens.push_back(Token(TokenType::NEQ, "!=", line)); }
                    else tokens.push_back(Token(TokenType::UNKNOWN, "!", line));
                    break;
                case '<':
                    advance();
                    if (current() == '=') { advance(); tokens.push_back(Token(TokenType::LE, "<=", line)); }
                    else tokens.push_back(Token(TokenType::LT, "<", line));
                    break;
                case '>':
                    advance();
                    if (current() == '=') { advance(); tokens.push_back(Token(TokenType::GE, ">=", line)); }
                    else tokens.push_back(Token(TokenType::GT, ">", line));
                    break;
                case '&':
                    advance();
                    tokens.push_back(Token(TokenType::AMP, "&", line));
                    break;
                case '(':
                    advance();
                    tokens.push_back(Token(TokenType::LPAREN, "(", line));
                    break;
                case ')':
                    advance();
                    tokens.push_back(Token(TokenType::RPAREN, ")", line));
                    break;
                case '{':
                    advance();
                    tokens.push_back(Token(TokenType::LBRACE, "{", line));
                    break;
                case '}':
                    advance();
                    tokens.push_back(Token(TokenType::RBRACE, "}", line));
                    break;
                case '[':
                    advance();
                    tokens.push_back(Token(TokenType::LBRACKET, "[", line));
                    break;
                case ']':
                    advance();
                    tokens.push_back(Token(TokenType::RBRACKET, "]", line));
                    break;
                case ';':
                    advance();
                    tokens.push_back(Token(TokenType::SEMICOLON, ";", line));
                    break;
                case ':':
                    advance();
                    tokens.push_back(Token(TokenType::COLON, ":", line));
                    break;
                case ',':
                    advance();
                    tokens.push_back(Token(TokenType::COMMA, ",", line));
                    break;
                case '.':
                    advance();
                    tokens.push_back(Token(TokenType::DOT, ".", line));
                    break;
                default:
                    advance();
                    tokens.push_back(Token(TokenType::UNKNOWN, std::string(1, c), line));
                    break;
            }
        }
    }
    
    return tokens;
}
