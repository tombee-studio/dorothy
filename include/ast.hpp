/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "./code.hpp"
#include "./llvm_gen.hpp"
#include "./vartype.hpp"
using std::cerr;
using std::endl;
using std::exception;
using std::ifstream;
using std::map;
using std::ostream;
using std::pair;
using std::runtime_error;
using std::string;
using std::to_string;
using std::vector;

class Expression;
class Statement;
class DeclVar;

// ===== Struct type info =====

struct FieldInfo {
  string name;
  VarType type;
  string struct_name; // non-empty when type == STRUCT
  bool is_nullable = false;
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
      if (fields[i].name == fname)
        return i;
    return -1;
  }
};

extern map<string, StructDefInfo> g_struct_defs;
extern string g_this_struct;
extern map<string, string> g_var_struct_types;

// ===== Class type info =====

struct MethodInfo {
  string name;
  vector<DeclVar *> params;
  VarType ret_type;
  string ret_type_name; // struct or class name
  bool is_ret_nullable = false;
  Statement *body = nullptr;
  bool is_abstract = false;
  bool is_override = false;
  int vtable_index = -1;
  string class_name;
};

struct ClassDefInfo {
  string name;
  string base_class;
  bool is_abstract = false;
  vector<FieldInfo> fields;
  vector<MethodInfo *> vtable_methods;
  map<string, MethodInfo *> methods;
  ConstructorInfo *constructor = nullptr;

  int fieldIndex(const string &fname) const {
    for (int i = 0; i < (int)fields.size(); i++)
      if (fields[i].name == fname)
        return i;
    return -1;
  }

  int getMethodVtableIndex(const string &mname) const {
    for (int i = 0; i < (int)vtable_methods.size(); i++) {
      if (vtable_methods[i]->name == mname)
        return i;
    }
    return -1;
  }

  MethodInfo *getMethod(const string &mname) const {
    auto it = methods.find(mname);
    return (it != methods.end()) ? it->second : nullptr;
  }
};

extern map<string, ClassDefInfo> g_class_defs;
extern string g_this_class;
extern map<string, string> g_var_class_types;

inline bool is_subclass_of(const string &derived, const string &base,
                           const map<string, ClassDefInfo> &defs) {
  if (derived == base)
    return true;
  auto it = defs.find(derived);
  if (it == defs.end())
    return false;
  if (it->second.base_class.empty())
    return false;
  return is_subclass_of(it->second.base_class, base, defs);
}

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
  string _struct_name; // non-empty when _type == VarType::STRUCT or CLASS
  bool _is_nullable;

public:
  DeclVar(string id, VarType type = VarType::LONG, bool is_const = false,
          string struct_name = "", bool is_nullable = false)
      : _id(id), _type(type), _is_const(is_const), _struct_name(struct_name),
        _is_nullable(is_nullable) {}

  VarType getType() const { return _type; }
  const string &getId() const { return _id; }
  bool isConst() const { return _is_const; }
  bool isNullable() const { return _is_nullable; }
  const string &getStructName() const { return _struct_name; }
  const string &getClassName() const { return _struct_name; }

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
                     string struct_name = "", bool is_nullable = false)
      : DeclVar(id, type, is_const, struct_name, is_nullable), _init(init) {}

  Expression *getInit() const { return _init; }
  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void llvm_emit(LLVMGenCtx &);
};

class DeclArrayVar : public DeclVar {
protected:
  int _num;

public:
  DeclArrayVar(string id, int num, VarType type = VarType::LONG,
               bool is_const = false)
      : DeclVar(id, type, is_const), _num(num) {}

  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void llvm_emit(LLVMGenCtx &);
};

class InitializedDeclArrayVar : public DeclArrayVar {
  vector<Expression *> _values;

public:
  InitializedDeclArrayVar(string id, int num, vector<Expression *> values,
                          VarType type = VarType::LONG, bool is_const = false)
      : DeclArrayVar(id, num, type, is_const), _values(values) {}

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
  bool _has_explicit_ret_type;
  bool _is_ret_nullable;

public:
  Function(string id, vector<DeclVar *> args, Statement *body,
           VarType ret_type = VarType::LONG, string ret_struct_name = "",
           bool has_explicit_ret_type = false, bool is_ret_nullable = false)
      : _id(id), _args(args), _body(body), _ret_type(ret_type),
        _ret_struct_name(ret_struct_name),
        _has_explicit_ret_type(has_explicit_ret_type),
        _is_ret_nullable(is_ret_nullable) {}
  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void llvm_emit(LLVMGenCtx &);
  void llvm_pre_register(LLVMGenCtx &);
  const string &getName() const { return _id; }
  VarType getRetType() const { return _ret_type; }
  const string &getRetStructName() const { return _ret_struct_name; }
  const string &getRetClassName() const { return _ret_struct_name; }
  bool hasExplicitRetType() const { return _has_explicit_ret_type; }
  bool isRetNullable() const { return _is_ret_nullable; }
  Statement *getBody() const { return _body; }
  virtual bool isImport() const { return false; }
};

class ImportFunction : public Function {
public:
  ImportFunction(string id, vector<DeclVar *> args)
      : Function(id, args, NULL) {}
  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void llvm_emit(LLVMGenCtx &) override;
  bool isImport() const override { return true; }
};

// Represents `import "header.h";` — imports a C header and emits correct LLVM
// declarations.
class ImportCHeader : public Function {
  string _header_path;

public:
  explicit ImportCHeader(string header_path)
      : Function("", {}, nullptr), _header_path(header_path) {}
  const string &getHeaderPath() const { return _header_path; }
  void print(ostream &, int) override {}
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override {}
  void llvm_emit(LLVMGenCtx &) override;
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

  DeclVar *getDecl() const { return _decl; }

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
  Statement *getTrueSt() const { return _truest; }
  Statement *getFalseSt() const { return _falsest; }
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
  Statement *getBody() const { return _body; }
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
  Statement *getBody() const { return _body; }
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
  Expression *getExpr() const { return _exp; }
  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void llvm_emit(LLVMGenCtx &);
};

class Block : public Statement {
  vector<Statement *> _statements;

public:
  explicit Block(vector<Statement *> statements) : _statements(statements) {}

  const vector<Statement *> &getStatements() const { return _statements; }

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
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int) = 0;
  virtual string llvm_rval(LLVMGenCtx &) = 0;
  virtual string llvm_lval(LLVMGenCtx &);
  // Returns the canonical computation type (LONG for ints, DOUBLE for floats)
  virtual VarType llvm_etype(LLVMGenCtx &) const { return VarType::LONG; }
  // Returns the declared type of this expression's target (for lvalues)
  virtual VarType llvm_declared_type(LLVMGenCtx &) const {
    return VarType::LONG;
  }
  // Returns the computation type for the bytecode compiler
  virtual VarType compile_type(map<string, int> &) const {
    return VarType::LONG;
  }
  // Returns the variable name if this is a simple variable reference, else ""
  virtual const string &getVarName() const {
    static string empty;
    return empty;
  }
  // Returns the static (compile-time) type of this expression for type
  // checking. var_types: variable name -> declared type func_ret_types:
  // function name -> declared return type
  virtual VarType
  static_type(const map<string, VarType> & /*var_types*/,
              const map<string, VarType> & /*func_ret_types*/) const {
    return VarType::LONG;
  }
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
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
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

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  VarType llvm_etype(LLVMGenCtx &ctx) const override {
    return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
  }
  VarType compile_type(map<string, int> &vars) const override {
    return promote_canonical(_left->compile_type(vars),
                             _right->compile_type(vars));
  }
  VarType static_type(const map<string, VarType> &vt,
                      const map<string, VarType> &frt) const override {
    return promote_canonical(_left->static_type(vt, frt),
                             _right->static_type(vt, frt));
  }
};

class SubExp : public Expression {
  Expression *_left;
  Expression *_right;

public:
  SubExp(Expression *left, Expression *right) : _left(left), _right(right) {}

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  VarType llvm_etype(LLVMGenCtx &ctx) const override {
    return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
  }
  VarType compile_type(map<string, int> &vars) const override {
    return promote_canonical(_left->compile_type(vars),
                             _right->compile_type(vars));
  }
  VarType static_type(const map<string, VarType> &vt,
                      const map<string, VarType> &frt) const override {
    return promote_canonical(_left->static_type(vt, frt),
                             _right->static_type(vt, frt));
  }
};

class MulExp : public Expression {
  Expression *_left;
  Expression *_right;

public:
  MulExp(Expression *left, Expression *right) : _left(left), _right(right) {}

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  VarType llvm_etype(LLVMGenCtx &ctx) const override {
    return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
  }
  VarType compile_type(map<string, int> &vars) const override {
    return promote_canonical(_left->compile_type(vars),
                             _right->compile_type(vars));
  }
  VarType static_type(const map<string, VarType> &vt,
                      const map<string, VarType> &frt) const override {
    return promote_canonical(_left->static_type(vt, frt),
                             _right->static_type(vt, frt));
  }
};

class DivExp : public Expression {
  Expression *_left;
  Expression *_right;

public:
  DivExp(Expression *left, Expression *right) : _left(left), _right(right) {}

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  VarType llvm_etype(LLVMGenCtx &ctx) const override {
    return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
  }
  VarType compile_type(map<string, int> &vars) const override {
    return promote_canonical(_left->compile_type(vars),
                             _right->compile_type(vars));
  }
  VarType static_type(const map<string, VarType> &vt,
                      const map<string, VarType> &frt) const override {
    return promote_canonical(_left->static_type(vt, frt),
                             _right->static_type(vt, frt));
  }
};

class ModExp : public Expression {
  Expression *_left;
  Expression *_right;

public:
  ModExp(Expression *left, Expression *right) : _left(left), _right(right) {}

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  VarType llvm_etype(LLVMGenCtx &ctx) const override {
    return promote_canonical(_left->llvm_etype(ctx), _right->llvm_etype(ctx));
  }
  VarType compile_type(map<string, int> &vars) const override {
    return promote_canonical(_left->compile_type(vars),
                             _right->compile_type(vars));
  }
  VarType static_type(const map<string, VarType> &vt,
                      const map<string, VarType> &frt) const override {
    return promote_canonical(_left->static_type(vt, frt),
                             _right->static_type(vt, frt));
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
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
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
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
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
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
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
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
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
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
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
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
  virtual string llvm_rval(LLVMGenCtx &);
};

class NullExp : public Expression {
public:
  NullExp() {}

  void print(ostream &os, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &, int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &, int) override;
  string llvm_rval(LLVMGenCtx &) override;
  VarType llvm_etype(LLVMGenCtx &) const override { return VarType::CLASS; }
  VarType llvm_declared_type(LLVMGenCtx &) const override { return VarType::CLASS; }
  VarType compile_type(map<string, int> &) const override { return VarType::CLASS; }
  VarType static_type(const map<string, VarType> &,
                      const map<string, VarType> &) const override {
    return VarType::CLASS;
  }
};

class IntExp : public Expression {
  int _int_val;

public:
  explicit IntExp(int int_val) : _int_val(int_val) {}

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  // Integer literals are int-sized (matching C's default type for integer
  // constants)
  VarType llvm_declared_type(LLVMGenCtx &) const override {
    return VarType::INT;
  }
};

class FloatExp : public Expression {
  double _float_val;

public:
  explicit FloatExp(double float_val) : _float_val(float_val) {}

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  VarType llvm_etype(LLVMGenCtx &) const override { return VarType::DOUBLE; }
  VarType compile_type(map<string, int> &) const override {
    return VarType::DOUBLE;
  }
  VarType static_type(const map<string, VarType> &,
                      const map<string, VarType> &) const override {
    return VarType::DOUBLE;
  }
};

class ArrayIndex : public Expression {
  Expression *_pointer;
  Expression *_index;

public:
  ArrayIndex(Expression *pointer, Expression *index)
      : _pointer(pointer), _index(index) {}

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  string llvm_lval(LLVMGenCtx &) override;
  VarType llvm_declared_type(LLVMGenCtx &ctx) const override;
  VarType llvm_etype(LLVMGenCtx &ctx) const override;
};

class Address : public Expression {
  Expression *_exp;

public:
  explicit Address(Expression *exp) : _exp(exp) {}

  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
  virtual string llvm_rval(LLVMGenCtx &);
  // Returns the raw LLVM ptr register (alloca ptr) without converting to i64.
  // Used for provenance-safe pointer passing to C functions.
  string llvm_ptr(LLVMGenCtx &ctx) { return _exp->llvm_lval(ctx); }
};

class RightSide : public Expression {
public:
  virtual void print(ostream &, int tab) {}
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int) {}
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
  virtual string llvm_rval(LLVMGenCtx &);
};

class Access : public Expression {
  Expression *_rightside;

public:
  explicit Access(Expression *rightside) : _rightside(rightside) {}

  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
  virtual string llvm_rval(LLVMGenCtx &);
  virtual string llvm_lval(LLVMGenCtx &);
};

class Variable : public Expression {
  string _id;

public:
  explicit Variable(string id) : _id(id) {}

  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  string llvm_lval(LLVMGenCtx &) override;
  VarType llvm_etype(LLVMGenCtx &) const override;
  VarType llvm_declared_type(LLVMGenCtx &) const override;
  VarType compile_type(map<string, int> &) const override;
  const string &getVarName() const override { return _id; }
  VarType static_type(const map<string, VarType> &var_types,
                      const map<string, VarType> &) const override {
    auto it = var_types.find(_id);
    return (it != var_types.end()) ? it->second : VarType::LONG;
  }
};

class CallFuncExp : public Expression {
  string _id;
  vector<Expression *> _args;

public:
  CallFuncExp(string id, vector<Expression *> args) : _id(id), _args(args) {}
  const string &getId() const { return _id; }
  const vector<Expression *> &getArgs() const { return _args; }
  void print(ostream &, int tab) override;
  void compile(vector<Code> &, map<string, int> &, map<string, int> &,
               int) override;
  void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                int) override;
  string llvm_rval(LLVMGenCtx &) override;
  VarType
  static_type(const map<string, VarType> &,
              const map<string, VarType> &func_ret_types) const override {
    auto it = func_ret_types.find(_id);
    return (it != func_ret_types.end()) ? it->second : VarType::LONG;
  }
};

// ===== Struct expression nodes =====

class StructInit : public Expression {
  string _struct_name;
  vector<Expression *> _args; // positional args matching constructor params

public:
  StructInit(string struct_name, vector<Expression *> args)
      : _struct_name(struct_name), _args(std::move(args)) {}

  const string &getStructName() const { return _struct_name; }
  const vector<Expression *> &getArgs() const { return _args; }

  virtual void print(ostream &, int tab);
  // StructInit cannot be used as a standalone expression; only via
  // InitializedDeclVar
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int) {}
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int) {}
  virtual string llvm_rval(LLVMGenCtx &) { return "0"; }
};

class MemberAccess : public Expression {
  Expression *_object;
  string _member;

public:
  MemberAccess(Expression *object, string member)
      : _object(object), _member(member) {}

  Expression *getObject() const { return _object; }
  const string &getMember() const { return _member; }

  // Returns the full dotted path from the root variable, e.g. "a.x" for
  // outer.a.x
  string getFieldPath() const {
    auto ma = dynamic_cast<const MemberAccess *>(_object);
    if (ma)
      return ma->getFieldPath() + "." + _member;
    return _member;
  }

  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
  virtual string llvm_rval(LLVMGenCtx &);
  virtual string llvm_lval(LLVMGenCtx &);
  virtual VarType llvm_etype(LLVMGenCtx &) const;
  virtual VarType llvm_declared_type(LLVMGenCtx &) const;
  virtual VarType compile_type(map<string, int> &) const;
  virtual const string &getVarName() const { return _object->getVarName(); }
};

class ThisExpr : public Expression {
public:
  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
  virtual string llvm_rval(LLVMGenCtx &);
  virtual string llvm_lval(LLVMGenCtx &);
  virtual const string &getVarName() const {
    static string this_key = "$this";
    return this_key;
  }
};

// ===== Class expression and statement nodes =====

class ClassInit : public Expression {
  string _class_name;
  vector<Expression *> _args;

public:
  ClassInit(string class_name, vector<Expression *> args)
      : _class_name(class_name), _args(std::move(args)) {}

  const string &getClassName() const { return _class_name; }
  const vector<Expression *> &getArgs() const { return _args; }

  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
  virtual string llvm_rval(LLVMGenCtx &);
  virtual VarType llvm_etype(LLVMGenCtx &) const { return VarType::LONG; }
  virtual VarType llvm_declared_type(LLVMGenCtx &) const {
    return VarType::CLASS;
  }
  virtual VarType static_type(const map<string, VarType> &,
                              const map<string, VarType> &) const {
    return VarType::CLASS;
  }
};

class CallMethodExp : public Expression {
  Expression *_object;
  string _method_name;
  vector<Expression *> _args;

public:
  CallMethodExp(Expression *object, string method_name,
                vector<Expression *> args)
      : _object(object), _method_name(method_name), _args(std::move(args)) {}

  Expression *getObject() const { return _object; }
  const string &getMethodName() const { return _method_name; }
  const vector<Expression *> &getArgs() const { return _args; }

  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void lcompile(vector<Code> &, map<string, int> &, map<string, int> &,
                        int);
  virtual string llvm_rval(LLVMGenCtx &);
  virtual VarType llvm_etype(LLVMGenCtx &ctx) const;
  virtual VarType llvm_declared_type(LLVMGenCtx &ctx) const;
};

class CallMethodSt : public Statement {
  CallMethodExp *_call;

public:
  explicit CallMethodSt(CallMethodExp *call) : _call(call) {}
  CallMethodExp *getCall() const { return _call; }

  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void llvm_emit(LLVMGenCtx &);
};

class ClassDef : public Node {
  ClassDefInfo _info;

public:
  explicit ClassDef(ClassDefInfo info) : _info(std::move(info)) {}
  const ClassDefInfo &getInfo() const { return _info; }

  virtual void print(ostream &, int tab);
  virtual void compile(vector<Code> &, map<string, int> &, map<string, int> &,
                       int);
  virtual void llvm_emit(LLVMGenCtx &);
};
