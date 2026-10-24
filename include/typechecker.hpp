/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <map>
#include <string>
#include <vector>

#include "./ast.hpp"
#include "./vartype.hpp"

using std::map;
using std::string;
using std::vector;

class TypeCheckError : public std::runtime_error {
 public:
    explicit TypeCheckError(const string &msg) : runtime_error(msg) {}
};

class TypeChecker {
    map<string, VarType> _func_ret_types;

    void check_expr(const Expression *expr,
                    const map<string, VarType> &var_types,
                    const map<string, bool> &const_vars,
                    const string &func_name);

    void check_stmt(const Statement *stmt,
                    map<string, VarType> &var_types,
                    map<string, bool> &const_vars,
                    VarType expected_ret,
                    const string &func_name);

 public:
    // Run static type checking on the entire program.
    // Nullability violations throw CompileError; other type errors throw TypeCheckError.
    void check(const vector<Function *> &program);
};

// Mandatory source-level nullability analysis, before either backend runs.
void check_null_safety(const vector<Function *> &program);
