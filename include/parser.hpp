/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <string>
#include <vector>

#include "./ast.hpp"
#include "./token.hpp"
#include "./utils.hpp"

using std::vector;

class ParseError : public runtime_error {
    Token _token;

 public:
    ParseError(const char *_Message, Token token)
        : runtime_error(_Message), _token(token) {}
    ParseError(string _Message, Token token)
        : runtime_error(_Message.c_str()), _token(token) {}
};

class Parser {
    int _pos;
    vector<Function *> _functions;
    map<string, StructDefInfo> _struct_defs;

 public:
    vector<Function *> parse(vector<Token> &tokens);
    const map<string, StructDefInfo>& getStructDefs() const { return _struct_defs; }

 private:
    Token consume(vector<Token> &, Token::Type);

    Function *parse_function(vector<Token> &);
    void parse_struct_def(vector<Token> &);
    vector<DeclVar *> parse_declargs(vector<Token> &);
    DeclVar *parse_declparam(vector<Token> &);
    DeclVar *parse_declvar(vector<Token> &);
    vector<Expression *> parse_array_initializer(vector<Token> &);
    Block *parse_block(vector<Token> &);

    Statement *parse_statement(vector<Token> &);
    Statement *parse_declvarst(vector<Token> &);
    Statement *parse_ifst(vector<Token> &);
    Statement *parse_whilest(vector<Token> &);
    Statement *parse_forst(vector<Token> &tokens);
    Statement *parse_returnst(vector<Token> &);
    Statement *parse_callst(vector<Token> &tokens);

    Expression *parse_expression(vector<Token> &);
    Expression *parse_assign(vector<Token> &);
    Expression *parse_eq(vector<Token> &);
    Expression *parse_add(vector<Token> &);
    Expression *parse_mul(vector<Token> &);
    Expression *parse_unary(vector<Token> &);
    Expression *parse_array_index(vector<Token> &);
    Expression *parse_term(vector<Token> &);
    Expression *parse_integer(vector<Token> &);
    Expression *parse_call(vector<Token> &);
    Expression *parse_struct_init(vector<Token> &);
    vector<Expression *> parse_arg(vector<Token> &);
};
