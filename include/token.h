#pragma once
#include <string>
#include <vector>

enum TokenType {
    // Literals
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,

    // Identifiers & Keywords
    TOKEN_IDENTIFIER,
    TOKEN_FUNC,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IMPORT,
    TOKEN_STRUCT,
    TOKEN_VAR,
    TOKEN_CONSTRUCTOR,
    TOKEN_THIS,
    TOKEN_VOID,

    // Type keywords
    TOKEN_INT_TYPE,
    TOKEN_CHAR_TYPE,
    TOKEN_LONG_TYPE,
    TOKEN_FLOAT_TYPE,
    TOKEN_DOUBLE_TYPE,

    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_AMPERSAND,
    TOKEN_ASSIGN,
    TOKEN_PLUS_ASSIGN,
    TOKEN_MINUS_ASSIGN,
    TOKEN_STAR_ASSIGN,
    TOKEN_SLASH_ASSIGN,

    // Comparison
    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,

    // Delimiters
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_DOT,

    TOKEN_EOF
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int col;
};
