#pragma once
#include <string>
#include <variant>

enum class TokenType {
    // Literals
    INTEGER,
    FLOAT_LITERAL,
    IDENTIFIER,
    STRING_LITERAL,
    
    // Keywords
    FUNC,
    RETURN,
    IF,
    ELSE,
    WHILE,
    FOR,
    IMPORT,
    STRUCT,
    VAR,
    CONSTRUCTOR,
    THIS,
    VOID,
    
    // Types
    TYPE_INT,
    TYPE_CHAR,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    
    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    ASSIGN,
    EQ,
    NEQ,
    LT,
    LE,
    GT,
    GE,
    AMP,
    
    // Punctuation
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    SEMICOLON,
    COLON,
    COMMA,
    DOT,
    
    // Compound assignment
    PLUS_ASSIGN,
    MINUS_ASSIGN,
    STAR_ASSIGN,
    SLASH_ASSIGN,
    
    // Special
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    
    Token(TokenType type, std::string value, int line = 0)
        : type(type), value(value), line(line) {}
};
