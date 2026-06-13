/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <iostream>
#include <string>

#include "./utils.hpp"
using std::string;

struct Token {
    enum Type {
        NONE = 0,
        TK_EOF = -1,
        TK_INT = 1,
        TK_ID = 2,
        TK_EQ = 3,
        TK_NE = 4,
        TK_LE = 5,
        TK_GE = 6,
        KW_INT = 7,
        KW_IF = 8,
        KW_ELSE = 9,
        KW_WHILE = 10,
        KW_RETURN = 11,
        KW_FUNC = 12,
        KW_IMPORT = 13,
        KW_FOR,
        KW_CHAR,
        KW_LONG,
        KW_FLOAT,
        KW_DOUBLE,
        TK_FLOAT,
        KW_VAR,
        KW_LET,
        KW_STRUCT,
        KW_CONSTRUCTOR,
        KW_THIS,
        TK_ARROW,     // ->
        TK_RAWSTRING, // "string literal" (raw content, not expanded to char array)
    } type;

    int int_val;
    double float_val;
    string id;

    static Token make_int(int value) {
        Token token;
        token.type = TK_INT;
        token.int_val = value;
        return token;
    }

    static Token make_float(double value) {
        Token token;
        token.type = TK_FLOAT;
        token.float_val = value;
        return token;
    }

    static Token make_id(string id) {
        Token token;
        token.type = TK_ID;
        token.id = id;
        return token;
    }

    static Token make_operator(int type) {
        Token token;
        token.type = (Token::Type)type;
        return token;
    }

    static Token make_string(string value) {
        Token token;
        token.type = TK_RAWSTRING;
        token.id = value;
        return token;
    }

    static Token none() {
        Token token;
        token.type = NONE;
        return token;
    }
};
