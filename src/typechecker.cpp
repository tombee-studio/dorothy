/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/typechecker.hpp"

static string vartype_name(VarType t) {
    switch (t) {
        case VarType::CHAR:   return "char";
        case VarType::INT:    return "int";
        case VarType::LONG:   return "long";
        case VarType::FLOAT:  return "float";
        case VarType::DOUBLE: return "double";
        case VarType::STRUCT: return "struct";
        default:              return "unknown";
    }
}

void TypeChecker::check_stmt(const Statement *stmt,
                              map<string, VarType> &var_types,
                              VarType expected_ret,
                              const string &func_name) {
    if (!stmt) return;

    if (auto *block = dynamic_cast<const Block *>(stmt)) {
        for (auto *s : block->getStatements())
            check_stmt(s, var_types, expected_ret, func_name);

    } else if (auto *ret = dynamic_cast<const ReturnSt *>(stmt)) {
        if (!ret->getExpr()) return;
        VarType expr_type = ret->getExpr()->static_type(var_types, _func_ret_types);
        bool expr_is_float = is_float_type(expr_type);
        bool ret_is_float  = is_float_type(expected_ret);
        if (expr_is_float && !ret_is_float) {
            throw TypeCheckError("function '" + func_name +
                                 "': cannot return float/double value from " +
                                 vartype_name(expected_ret) + "-returning function");
        }
        if (!expr_is_float && ret_is_float) {
            // integer returned from float function — allowed (implicit widening)
        }

    } else if (auto *if_st = dynamic_cast<const IfSt *>(stmt)) {
        check_stmt(if_st->getTrueSt(), var_types, expected_ret, func_name);
        check_stmt(if_st->getFalseSt(), var_types, expected_ret, func_name);

    } else if (auto *while_st = dynamic_cast<const WhileSt *>(stmt)) {
        check_stmt(while_st->getBody(), var_types, expected_ret, func_name);

    } else if (auto *for_st = dynamic_cast<const ForSt *>(stmt)) {
        check_stmt(for_st->getBody(), var_types, expected_ret, func_name);

    } else if (auto *decl_st = dynamic_cast<const DeclVarSt *>(stmt)) {
        DeclVar *decl = decl_st->getDecl();
        VarType t = decl->getType();
        // INFERRED is resolved at codegen; record as LONG for type-checking purposes
        if (t == VarType::INFERRED) t = VarType::LONG;
        var_types[decl->getId()] = t;
    }
    // CallFuncSt, ExpressionSt — no return to check
}

void TypeChecker::check(const vector<Function *> &program) {
    // First pass: collect all function return types
    _func_ret_types.clear();
    for (auto *f : program)
        _func_ret_types[f->getName()] = f->getRetType();

    // Second pass: check return statements in typed functions
    for (auto *f : program) {
        if (f->isImport() || !f->hasExplicitRetType()) continue;
        VarType expected = f->getRetType();
        if (expected == VarType::STRUCT) continue;  // struct return type checking is deferred
        map<string, VarType> var_types;
        check_stmt(f->getBody(), var_types, expected, f->getName());
    }
}
