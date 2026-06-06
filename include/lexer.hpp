/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <vector>
#include <string>

#include "./token.hpp"
using std::runtime_error;
using std::vector;

class LexerError : public runtime_error {
 public:
    explicit LexerError(const char* message) : runtime_error(message) {}
};

class Lexer {
    vector<Token> tokens;

 public:
    vector<Token> lex(string);

 private:
    void tokenize(string, int*);
    bool skip(string, int*);
    bool tokenize_int(string, int*);
    bool tokenize_float(string, int*);
    bool tokenize_id(string, int*);
    bool tokenize_char(string, int*);
    bool tokenize_str(string, int*);
    bool tokenize_keyword(string p, int* ppos, string keyword,
                          Token::Type type);
    bool tokenize_operator(string, int*, char);
};
