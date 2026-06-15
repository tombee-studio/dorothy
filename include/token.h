#pragma once
#include <string>
#include <variant>

enum class TokenType {
    // Literals
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    
    // Identifiers
    IDENTIFIER,
    
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
    THIS,
    CONSTRUCTOR,
    VOID,
    NEW,
    
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
    AMPERSAND,
    
    // Comparison
    EQ,
    NEQ,
    LT,
    LE,
    GT,
    GE,
    
    // Assignment
    ASSIGN,
    PLUS_ASSIGN,
    MINUS_ASSIGN,
    STAR_ASSIGN,
    SLASH_ASSIGN,
    
    // Delimiters
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
    
    // Special
    EOF_TOKEN,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int col;
};
