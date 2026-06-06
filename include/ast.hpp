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
using std::endl;
using std::cerr;
using std::ifstream;
using std::to_string;
using std::exception;
using std::runtime_error;

class Expression;
class Statement;

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

 public:
    DeclVar(string id, VarType type = VarType::LONG) : _id(id), _type(type) {}

    VarType getType() const { return _type; }
    const string& getId() const { return _id; }

    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
    virtual void llvm_param(LLVMGenCtx &, const string &);
};

class DeclArrayVar : public DeclVar {
 protected:
    int _num;

 public:
    DeclArrayVar(string id, int num, VarType type = VarType::LONG)
        : DeclVar(id, type), _num(num) {}

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

 public:
    Function(string id, vector<DeclVar *> args, Statement *body)
        : _id(id), _args(args), _body(body) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void llvm_emit(LLVMGenCtx &);
    const string &getName() const { return _id; }
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
};

class CallFuncExp : public Expression {
    string _id;
    vector<Expression *> _args;

 public:
    CallFuncExp(string id, vector<Expression *> args) : _id(id), _args(args) {}
    virtual void print(ostream &, int tab);
    virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                         int);
    virtual void lcompile(vector<Code> &, map<string, int> &,
                          map<string, int> &, int);
    virtual string llvm_rval(LLVMGenCtx &);
};
