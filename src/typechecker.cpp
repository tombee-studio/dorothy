/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/typechecker.hpp"

static string vartype_name(VarType t) {
    switch (t) {
        case VarType::CHAR:   return "char";
        case VarType::INT:    return "int";
        case VarType::LONG:   return "long";
        case VarType::FLOAT:  return "float";
        case VarType::DOUBLE: return "double";
        case VarType::STRING: return "string";
        case VarType::STRUCT: return "struct";
        case VarType::CLASS:  return "class";
        default:              return "unknown";
    }
}

void TypeChecker::check_expr(const Expression *expr,
                             const map<string, VarType> &var_types,
                             const map<string, bool> &const_vars,
                             const string &func_name) {
    if (!expr) return;

    if (auto *assign = dynamic_cast<const Assign *>(expr)) {
        if (assign->getLeftSide()) {
            const string &varname = assign->getLeftSide()->getVarName();
            if (!varname.empty()) {
                auto it = const_vars.find(varname);
                if (it != const_vars.end() && it->second) {
                    throw TypeCheckError("cannot assign to constant: " + varname);
                }
            }
            check_expr(assign->getLeftSide(), var_types, const_vars, func_name);
        }
        if (assign->getExpr()) {
            check_expr(assign->getExpr(), var_types, const_vars, func_name);
        }
    }
}

void TypeChecker::check_stmt(const Statement *stmt,
                              map<string, VarType> &var_types,
                              map<string, bool> &const_vars,
                              VarType expected_ret,
                              const string &func_name) {
    if (!stmt) return;

    if (auto *block = dynamic_cast<const Block *>(stmt)) {
        for (auto *s : block->getStatements())
            check_stmt(s, var_types, const_vars, expected_ret, func_name);

    } else if (auto *ret = dynamic_cast<const ReturnSt *>(stmt)) {
        if (!ret->getExpr()) return;
        check_expr(ret->getExpr(), var_types, const_vars, func_name);
        if (expected_ret == VarType::INFERRED || expected_ret == VarType::STRUCT) return;
        VarType expr_type = ret->getExpr()->static_type(var_types, _func_ret_types);
        bool expr_is_str = is_string_type(expr_type);
        bool ret_is_str = is_string_type(expected_ret);
        if (expr_is_str && !ret_is_str) {
            throw TypeCheckError("function '" + func_name +
                                 "': cannot return string value from " +
                                 vartype_name(expected_ret) + "-returning function");
        }
        if (!expr_is_str && ret_is_str) {
            throw TypeCheckError("function '" + func_name +
                                 "': cannot return non-string value from string-returning function");
        }
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
        check_expr(if_st->getCond(), var_types, const_vars, func_name);
        check_stmt(if_st->getTrueSt(), var_types, const_vars, expected_ret, func_name);
        check_stmt(if_st->getFalseSt(), var_types, const_vars, expected_ret, func_name);

    } else if (auto *while_st = dynamic_cast<const WhileSt *>(stmt)) {
        check_expr(while_st->getCond(), var_types, const_vars, func_name);
        check_stmt(while_st->getBody(), var_types, const_vars, expected_ret, func_name);

    } else if (auto *for_st = dynamic_cast<const ForSt *>(stmt)) {
        check_expr(for_st->getInit(), var_types, const_vars, func_name);
        check_expr(for_st->getCond(), var_types, const_vars, func_name);
        check_expr(for_st->getProceed(), var_types, const_vars, func_name);
        check_stmt(for_st->getBody(), var_types, const_vars, expected_ret, func_name);

    } else if (auto *decl_st = dynamic_cast<const DeclVarSt *>(stmt)) {
        DeclVar *decl = decl_st->getDecl();
        VarType t = decl->getType();
        bool is_const = decl->isConst();
        const_vars[decl->getId()] = is_const;

        auto *init_decl = dynamic_cast<const InitializedDeclVar *>(decl);
        if (init_decl && init_decl->getInit()) {
            check_expr(init_decl->getInit(), var_types, const_vars, func_name);
            VarType init_t = init_decl->getInit()->static_type(var_types, _func_ret_types);
            if (t == VarType::STRING && !is_string_type(init_t)) {
                throw TypeCheckError("cannot initialize string variable '" + decl->getId() + "' with non-string value");
            }
            if (t != VarType::STRING && t != VarType::INFERRED && is_string_type(init_t)) {
                throw TypeCheckError("cannot initialize " + vartype_name(t) + " variable '" + decl->getId() + "' with string value");
            }
            if (t == VarType::INFERRED) {
                t = init_t;
            }
        }
        if (t == VarType::INFERRED) t = VarType::LONG;
        var_types[decl->getId()] = t;

    } else if (auto *expr_st = dynamic_cast<const ExpressionSt *>(stmt)) {
        check_expr(expr_st->getExpr(), var_types, const_vars, func_name);

    } else if (auto *call_st = dynamic_cast<const CallFuncSt *>(stmt)) {
        for (auto *arg : call_st->getArgs()) {
            check_expr(arg, var_types, const_vars, func_name);
        }
    }
}

void TypeChecker::check(const vector<Function *> &program) {
    // First pass: collect all function return types
    _func_ret_types.clear();
    for (auto *f : program)
        _func_ret_types[f->getName()] = f->getRetType();

    // Second pass: check return statements and constant assignments in functions
    for (auto *f : program) {
        if (f->isImport()) continue;
        VarType expected = f->hasExplicitRetType() ? f->getRetType() : VarType::INFERRED;
        map<string, VarType> var_types;
        map<string, bool> const_vars;
        for (auto *arg : f->getArgs()) {
            var_types[arg->getId()] = arg->getType();
            const_vars[arg->getId()] = arg->isConst();
        }
        check_stmt(f->getBody(), var_types, const_vars, expected, f->getName());
    }
}
