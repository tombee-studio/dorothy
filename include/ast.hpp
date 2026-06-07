/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <string>

#include "./code.hpp"
#include "./llvm_gen.hpp"
#include "./utils.hpp"
#include "./vartype.hpp"
using std::vector;
using std::ostream;
using std::string;
using std::map;
using std::pair;
using std::endl;
using std::cerr;
using std::ifstream;
using std::to_string;
using std::exception;
using std::runtime_error;

class Expression;
class Statement;

// ===== Struct type info =====

struct FieldInfo {
    string name;
    VarType type;
};

struct ConstructorInfo {
    vector<pair<string, VarType>> params;
    Statement *body;
};

struct StructDefInfo {
    string name;
    vector<FieldInfo> fields;
    ConstructorInfo *constructor = nullptr;

    int fieldIndex(const string &fname) const {
        for (int i = 0; i < (int)fields.size(); i++)
            if (fields[i].name == fname) return i;
        return -1;
    }
};

extern map<string, StructDefInfo> g_struct_defs;
extern string g_this_struct;
extern map<string, string> g_var_struct_types;

class CompileError : public std::runtime_error {
 public:
    explicit CompileError(const char *_Message) : runtime_error(_Message) {}
};

class Node {
 public:
    virtual void print(ostream &, int tab) = 0;
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int) = 0;
    virtual void llvm_emit(LLVMGenCtx &) {}

    static void addTab(ostream &, int tab);
};

class DeclVar : public Node {
 protected:
    string _id;
    VarType _type;
    bool _is_const;
    string _struct_name;  // non-empty when _type == VarType::STRUCT

 public:
    DeclVar(string id, VarType type = VarType::LONG, bool is_const = false,
            string struct_name = "")
        : _id(id), _type(type), _is_const(is_const), _struct_name(struct_name) {}

    VarType getType() const { return _type; }
    const string& getId() const { return _id; }
    bool isConst() const { return _is_const; }
    const string& getStructName() const { return _struct_name; }

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
    virtual void llvm_param(LLVMGenCtx &, const string &);
};

class InitializedDeclVar : public DeclVar {
    Expression *_init;

 public:
    InitializedDeclVar(string id, VarType type, bool is_const, Expression *init,
                       string struct_name = "")
        : DeclVar(id, type, is_const, struct_name), _init(init) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class DeclArrayVar : public DeclVar {
 protected:
    int _num;

 public:
    DeclArrayVar(string id, int num, VarType type = VarType::LONG)
        : DeclVar(id, type, false), _num(num) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class InitializedDeclArrayVar : public DeclArrayVar {
    vector<Expression *> _values;

 public:
    InitializedDeclArrayVar(string id, int num, vector<Expression *> values,
                            VarType type = VarType::LONG)
        : DeclArrayVar(id, num, type), _values(values) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class Function : public Node {
 protected:
    string _id;
    vector<DeclVar *> _args;
    Statement *_body;
    VarType _ret_type;
    string _ret_struct_name;

 public:
    Function(string id, vector<DeclVar *> args, Statement *body,
             VarType ret_type = VarType::LONG, string ret_struct_name = "")
        : _id(id), _args(args), _body(body),
          _ret_type(ret_type), _ret_struct_name(ret_struct_name) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
    void llvm_pre_register(LLVMGenCtx &);
    const string &getName() const { return _id; }
    VarType getRetType() const { return _ret_type; }
    const string &getRetStructName() const { return _ret_struct_name; }
    virtual bool isImport() const { return false; }
};

class ImportFunction : public Function {
 public:
    ImportFunction(string id, vector<DeclVar *> args)
        : Function(id, args, NULL) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
    bool isImport() const override { return true; }
};

class Statement : public Node {
 public:
    virtual void print(ostream &, int tab) {}
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int) {}
};

class DeclVarSt : public Statement {
    DeclVar *_decl;

 public:
    explicit DeclVarSt(DeclVar *decl) : _decl(decl) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class IfSt : public Statement {
    Expression *_cond;
    Statement *_truest;
    Statement *_falsest;

 public:
    IfSt(Expression *cond, Statement *truest, Statement *falsest)
        : _cond(cond), _truest(truest), _falsest(falsest) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class WhileSt : public Statement {
    Expression *_cond;
    Statement *_body;

 public:
    WhileSt(Expression *cond, Statement *body) : _cond(cond), _body(body) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class ForSt : public Statement {
    Expression *_init;
    Expression *_cond;
    Expression *_proceed;
    Statement *_body;

 public:
    ForSt(Expression *init, Expression *cond, Expression *proceed,
          Statement *body)
        : _init(init), _cond(cond), _proceed(proceed), _body(body) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class CallFuncSt : public Statement {
    string _id;
    vector<Expression *> _args;

 public:
    CallFuncSt(string id, vector<Expression *> args) : _id(id), _args(args) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class ReturnSt : public Statement {
    Expression *_exp;

 public:
    explicit ReturnSt(Expression *exp) : _exp(exp) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class Block : public Statement {
    vector<Statement *> _statements;

 public:
    explicit Block(vector<Statement *> statements) : _statements(statements) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class Expression : public Node {
 public:
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int) = 0;
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int) = 0;
    virtual string llvm_rval(LLVMGenCtx &) = 0;
    virtual string llvm_lval(LLVMGenCtx &);
    // Returns the canonical computation type (LONG for ints, DOUBLE for floats)
    virtual VarType llvm_etype(LLVMGenCtx &) const { return VarType::LONG; }
    // Returns the declared type of this expression's target (for lvalues)
    virtual VarType llvm_declared_type(LLVMGenCtx &) const { return VarType::LONG; }
    // Returns the computation type for the bytecode compiler
    virtual VarType compile_type(map<string, int> &) const { return VarType::LONG; }
    // Returns the variable name if this is a simple variable reference, else ""
    virtual const string& getVarName() const { static string empty; return empty; }
};

class ExpressionSt : public Statement {
    Expression *_exp;

 public:
    explicit ExpressionSt(Expression *exp) : _exp(exp) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
};

class Assign : public Expression {
    Expression *_leftside;
    Expression *_expr;

 public:
    Assign(Expression *leftside, Expression *expr)
        : _leftside(leftside), _expr(expr) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &ctx) const {
        return canonical_type(_leftside->llvm_declared_type(ctx));
    }
    virtual VarType compile_type(map<string, int> &vars) const {
        return _leftside->compile_type(vars);
    }
};

class AddExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    AddExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &ctx) const {
        return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
    }
    virtual VarType compile_type(map<string, int> &vars) const {
        return promote_canonical(_left->compile_type(vars), _right->compile_type(vars));
    }
};

class SubExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    SubExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &ctx) const {
        return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
    }
    virtual VarType compile_type(map<string, int> &vars) const {
        return promote_canonical(_left->compile_type(vars), _right->compile_type(vars));
    }
};

class MulExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    MulExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &ctx) const {
        return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
    }
    virtual VarType compile_type(map<string, int> &vars) const {
        return promote_canonical(_left->compile_type(vars), _right->compile_type(vars));
    }
};

class DivExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    DivExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &ctx) const {
        return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
    }
    virtual VarType compile_type(map<string, int> &vars) const {
        return promote_canonical(_left->compile_type(vars), _right->compile_type(vars));
    }
};

class ModExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    ModExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &ctx) const {
        return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
    }
    virtual VarType compile_type(map<string, int> &vars) const {
        return promote_canonical(_left->compile_type(vars), _right->compile_type(vars));
    }
};

class EQExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    EQExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class NEExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    NEExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class LTExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    LTExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class LEExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    LEExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class GTExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    GTExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class GEExp : public Expression {
    Expression *_left;
    Expression *_right;

 public:
    GEExp(Expression *left, Expression *right) : _left(left), _right(right) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class IntExp : public Expression {
    int _int_val;

 public:
    explicit IntExp(int int_val) : _int_val(int_val) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class FloatExp : public Expression {
    double _float_val;

 public:
    explicit FloatExp(double float_val) : _float_val(float_val) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &) const { return VarType::DOUBLE; }
    virtual VarType compile_type(map<string, int> &) const { return VarType::DOUBLE; }
};

class ArrayIndex : public Expression {
    Expression *_pointer;
    Expression *_index;

 public:
    ArrayIndex(Expression *pointer, Expression *index)
        : _pointer(pointer), _index(index) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual string llvm_lval(LLVMGenCtx &);
};

class Address : public Expression {
    Expression *_exp;

 public:
    explicit Address(Expression *exp) : _exp(exp) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class RightSide : public Expression {
 public:
    virtual void print(ostream &, int tab) {}
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int) {}
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

class Access : public Expression {
    Expression *_rightside;

 public:
    explicit Access(Expression *rightside) : _rightside(rightside) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual string llvm_lval(LLVMGenCtx &);
};

class Variable : public Expression {
    string _id;

 public:
    explicit Variable(string id) : _id(id) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual string llvm_lval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &) const;
    virtual VarType llvm_declared_type(LLVMGenCtx &) const;
    virtual VarType compile_type(map<string, int> &) const;
    virtual const string& getVarName() const { return _id; }
};

class CallFuncExp : public Expression {
    string _id;
    vector<Expression *> _args;

 public:
    CallFuncExp(string id, vector<Expression *> args) : _id(id), _args(args) {}
    const string &getId() const { return _id; }
    const vector<Expression *> &getArgs() const { return _args; }
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};

// ===== Struct expression nodes =====

class StructInit : public Expression {
    string _struct_name;
    vector<Expression *> _args;  // positional args matching constructor params

 public:
    StructInit(string struct_name, vector<Expression *> args)
        : _struct_name(struct_name), _args(std::move(args)) {}

    const string& getStructName() const { return _struct_name; }
    const vector<Expression *>& getArgs() const { return _args; }

    virtual void print(ostream &, int tab);
    // StructInit cannot be used as a standalone expression; only via InitializedDeclVar
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &, int) {}
    virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &, int) {}
    virtual string llvm_rval(LLVMGenCtx &) { return "0"; }
};

class MemberAccess : public Expression {
    Expression *_object;
    string _member;

 public:
    MemberAccess(Expression *object, string member)
        : _object(object), _member(member) {}

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &, int);
    virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual string llvm_lval(LLVMGenCtx &);
    virtual VarType llvm_etype(LLVMGenCtx &) const;
    virtual VarType llvm_declared_type(LLVMGenCtx &) const;
    virtual VarType compile_type(map<string, int> &) const;
    virtual const string& getVarName() const { return _object->getVarName(); }
};

class ThisExpr : public Expression {
 public:
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &, int);
    virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
    virtual string llvm_lval(LLVMGenCtx &);
    virtual const string& getVarName() const {
        static string this_key = "$this";
        return this_key;
    }
};
