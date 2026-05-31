/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/ast.hpp"

using std::to_string;
using std::string;
using std::vector;

// ===== Expression base =====

string Expression::llvm_lval(LLVMGenCtx& ctx) {
    throw CompileError("expression cannot be used as left-value");
}

// ===== DeclVar =====

void DeclVar::llvm_emit(LLVMGenCtx& ctx) {
    int n = ctx.counter++;
    string ptr = "%" + _id + ".addr." + to_string(n);
    ctx.out << "  " << ptr << " = alloca i64\n";
    ctx.out << "  store i64 0, ptr " << ptr << "\n";
    ctx.vars[_id] = ptr;
}

void DeclVar::llvm_param(LLVMGenCtx& ctx, const string& param_reg) {
    int n = ctx.counter++;
    string ptr = "%" + _id + ".addr." + to_string(n);
    ctx.out << "  " << ptr << " = alloca i64\n";
    ctx.out << "  store i64 " << param_reg << ", ptr " << ptr << "\n";
    ctx.vars[_id] = ptr;
}

// ===== DeclArrayVar =====

void DeclArrayVar::llvm_emit(LLVMGenCtx& ctx) {
    int n = ctx.counter++;
    string data = "%" + _id + ".data." + to_string(n);
    string base = "%" + _id + ".base." + to_string(n);
    string ptr = "%" + _id + ".addr." + to_string(n);
    ctx.out << "  " << data << " = alloca i64, i64 " << _num << "\n";
    ctx.out << "  " << base << " = ptrtoint ptr " << data << " to i64\n";
    ctx.out << "  " << ptr << " = alloca i64\n";
    ctx.out << "  store i64 " << base << ", ptr " << ptr << "\n";
    ctx.vars[_id] = ptr;
}

// ===== InitializedDeclArrayVar =====

void InitializedDeclArrayVar::llvm_emit(LLVMGenCtx& ctx) {
    DeclArrayVar::llvm_emit(ctx);

    string base = ctx.fresh(_id + ".init.base");
    ctx.out << "  " << base << " = load i64, ptr " << ctx.vars[_id] << "\n";
    string arr_ptr = ctx.fresh(_id + ".init.ptr");
    ctx.out << "  " << arr_ptr << " = inttoptr i64 " << base << " to ptr\n";

    for (int i = 0; i < (int)_values.size(); i++) {
        string val = _values[i]->llvm_rval(ctx);
        string elem = ctx.fresh("elem");
        ctx.out << "  " << elem << " = getelementptr i64, ptr " << arr_ptr
                << ", i64 " << i << "\n";
        ctx.out << "  store i64 " << val << ", ptr " << elem << "\n";
    }
}

// ===== DeclVarSt =====

void DeclVarSt::llvm_emit(LLVMGenCtx& ctx) { _decl->llvm_emit(ctx); }

// ===== Block =====

void Block::llvm_emit(LLVMGenCtx& ctx) {
    for (auto st : _statements) {
        if (ctx.terminated) break;
        st->llvm_emit(ctx);
    }
}

// ===== IfSt =====

void IfSt::llvm_emit(LLVMGenCtx& ctx) {
    string then_label = ctx.freshLabel("if.then");
    string else_label = _falsest ? ctx.freshLabel("if.else") : "";
    string end_label = ctx.freshLabel("if.end");

    string cond = _cond->llvm_rval(ctx);
    string cond_bool = ctx.fresh("cond.br");
    ctx.out << "  " << cond_bool << " = icmp ne i64 " << cond << ", 0\n";
    ctx.out << "  br i1 " << cond_bool << ", label %" << then_label
            << ", label %" << (_falsest ? else_label : end_label) << "\n";

    ctx.startBlock(then_label);
    _truest->llvm_emit(ctx);
    bool then_terminated = ctx.terminated;
    if (!then_terminated) {
        ctx.out << "  br label %" << end_label << "\n";
        ctx.terminated = true;
    }

    bool else_terminated = false;
    if (_falsest) {
        ctx.startBlock(else_label);
        _falsest->llvm_emit(ctx);
        else_terminated = ctx.terminated;
        if (!else_terminated) {
            ctx.out << "  br label %" << end_label << "\n";
            ctx.terminated = true;
        }
    }

    if (!_falsest || !then_terminated || !else_terminated) {
        ctx.startBlock(end_label);
    } else {
        ctx.terminated = true;
    }
}

// ===== WhileSt =====

void WhileSt::llvm_emit(LLVMGenCtx& ctx) {
    string cond_label = ctx.freshLabel("while.cond");
    string body_label = ctx.freshLabel("while.body");
    string end_label = ctx.freshLabel("while.end");

    ctx.out << "  br label %" << cond_label << "\n";

    ctx.startBlock(cond_label);
    string cond = _cond->llvm_rval(ctx);
    string cond_bool = ctx.fresh("cond.br");
    ctx.out << "  " << cond_bool << " = icmp ne i64 " << cond << ", 0\n";
    ctx.out << "  br i1 " << cond_bool << ", label %" << body_label
            << ", label %" << end_label << "\n";

    ctx.startBlock(body_label);
    _body->llvm_emit(ctx);
    if (!ctx.terminated) {
        ctx.out << "  br label %" << cond_label << "\n";
    }

    ctx.startBlock(end_label);
}

// ===== ForSt =====

void ForSt::llvm_emit(LLVMGenCtx& ctx) {
    string cond_label = ctx.freshLabel("for.cond");
    string body_label = ctx.freshLabel("for.body");
    string step_label = ctx.freshLabel("for.step");
    string end_label = ctx.freshLabel("for.end");

    _init->llvm_rval(ctx);
    ctx.out << "  br label %" << cond_label << "\n";

    ctx.startBlock(cond_label);
    string cond = _cond->llvm_rval(ctx);
    string cond_bool = ctx.fresh("cond.br");
    ctx.out << "  " << cond_bool << " = icmp ne i64 " << cond << ", 0\n";
    ctx.out << "  br i1 " << cond_bool << ", label %" << body_label
            << ", label %" << end_label << "\n";

    ctx.startBlock(body_label);
    _body->llvm_emit(ctx);
    if (!ctx.terminated) {
        ctx.out << "  br label %" << step_label << "\n";
    }

    ctx.startBlock(step_label);
    _proceed->llvm_rval(ctx);
    ctx.out << "  br label %" << cond_label << "\n";

    ctx.startBlock(end_label);
}

// ===== ReturnSt =====

void ReturnSt::llvm_emit(LLVMGenCtx& ctx) {
    string val = _exp->llvm_rval(ctx);
    if (ctx.current_function == "main") {
        string trunc_reg = ctx.fresh("ret.i32");
        ctx.out << "  " << trunc_reg << " = trunc i64 " << val << " to i32\n";
        ctx.out << "  ret i32 " << trunc_reg << "\n";
    } else {
        ctx.out << "  ret i64 " << val << "\n";
    }
    ctx.terminated = true;
}

// ===== CallFuncSt =====

void CallFuncSt::llvm_emit(LLVMGenCtx& ctx) {
    vector<string> arg_regs;
    for (auto arg : _args) {
        arg_regs.push_back(arg->llvm_rval(ctx));
    }
    string reg = ctx.fresh("call");
    bool is_imported = ctx.defined_funcs.find(_id) == ctx.defined_funcs.end();
    if (is_imported) {
        ctx.out << "  " << reg << " = call i64 (...) @" << _id << "(";
    } else {
        ctx.out << "  " << reg << " = call i64 @" << _id << "(";
    }
    for (int i = 0; i < (int)arg_regs.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << "i64 " << arg_regs[i];
    }
    ctx.out << ")\n";
}

// ===== ExpressionSt =====

void ExpressionSt::llvm_emit(LLVMGenCtx& ctx) { _exp->llvm_rval(ctx); }

// ===== Function =====

void Function::llvm_emit(LLVMGenCtx& ctx) {
    ctx.vars.clear();
    ctx.terminated = false;
    ctx.current_function = _id;

    bool is_main = (_id == "main");
    ctx.out << "define " << (is_main ? "i32" : "i64") << " @" << _id << "(";
    for (int i = 0; i < (int)_args.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << "i64 %param." << i;
    }
    ctx.out << ") {\nentry:\n";

    for (int i = 0; i < (int)_args.size(); i++) {
        _args[i]->llvm_param(ctx, "%param." + to_string(i));
    }

    _body->llvm_emit(ctx);

    if (!ctx.terminated) {
        ctx.out << (is_main ? "  ret i32 0\n" : "  ret i64 0\n");
    }

    ctx.out << "}\n\n";
}

// ===== ImportFunction =====

void ImportFunction::llvm_emit(LLVMGenCtx& ctx) {
    ctx.out << "declare i64 @" << _id << "(...)\n\n";
}

// ===== Assign =====

string Assign::llvm_rval(LLVMGenCtx& ctx) {
    string ptr = _leftside->llvm_lval(ctx);
    string val = _expr->llvm_rval(ctx);
    ctx.out << "  store i64 " << val << ", ptr " << ptr << "\n";
    return val;
}

// ===== Binary arithmetic =====

string AddExp::llvm_rval(LLVMGenCtx& ctx) {
    string l = _left->llvm_rval(ctx);
    string r = _right->llvm_rval(ctx);
    string reg = ctx.fresh();
    ctx.out << "  " << reg << " = add i64 " << l << ", " << r << "\n";
    return reg;
}

string SubExp::llvm_rval(LLVMGenCtx& ctx) {
    string l = _left->llvm_rval(ctx);
    string r = _right->llvm_rval(ctx);
    string reg = ctx.fresh();
    ctx.out << "  " << reg << " = sub i64 " << l << ", " << r << "\n";
    return reg;
}

string MulExp::llvm_rval(LLVMGenCtx& ctx) {
    string l = _left->llvm_rval(ctx);
    string r = _right->llvm_rval(ctx);
    string reg = ctx.fresh();
    ctx.out << "  " << reg << " = mul i64 " << l << ", " << r << "\n";
    return reg;
}

string DivExp::llvm_rval(LLVMGenCtx& ctx) {
    string l = _left->llvm_rval(ctx);
    string r = _right->llvm_rval(ctx);
    string reg = ctx.fresh();
    ctx.out << "  " << reg << " = sdiv i64 " << l << ", " << r << "\n";
    return reg;
}

string ModExp::llvm_rval(LLVMGenCtx& ctx) {
    string l = _left->llvm_rval(ctx);
    string r = _right->llvm_rval(ctx);
    string reg = ctx.fresh();
    ctx.out << "  " << reg << " = srem i64 " << l << ", " << r << "\n";
    return reg;
}

// ===== Comparison operators =====

static string emitCmp(LLVMGenCtx& ctx, Expression* left, Expression* right,
                      const string& pred) {
    string l = left->llvm_rval(ctx);
    string r = right->llvm_rval(ctx);
    string cmp = ctx.fresh("cmp");
    string reg = ctx.fresh();
    ctx.out << "  " << cmp << " = icmp " << pred << " i64 " << l << ", " << r
            << "\n";
    ctx.out << "  " << reg << " = zext i1 " << cmp << " to i64\n";
    return reg;
}

string EQExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "eq");
}
string NEExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "ne");
}
string LTExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "slt");
}
string LEExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "sle");
}
string GTExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "sgt");
}
string GEExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "sge");
}

// ===== IntExp =====

string IntExp::llvm_rval(LLVMGenCtx& ctx) { return to_string(_int_val); }

// ===== Variable =====

string Variable::llvm_rval(LLVMGenCtx& ctx) {
    auto it = ctx.vars.find(_id);
    if (it == ctx.vars.end()) {
        throw CompileError(("undefined variable: " + _id).c_str());
    }
    string reg = ctx.fresh(_id);
    ctx.out << "  " << reg << " = load i64, ptr " << it->second << "\n";
    return reg;
}

string Variable::llvm_lval(LLVMGenCtx& ctx) {
    auto it = ctx.vars.find(_id);
    if (it == ctx.vars.end()) {
        throw CompileError(("undefined variable: " + _id).c_str());
    }
    return it->second;
}

// ===== ArrayIndex =====

string ArrayIndex::llvm_rval(LLVMGenCtx& ctx) {
    string base = _pointer->llvm_rval(ctx);
    string idx = _index->llvm_rval(ctx);
    string arr_ptr = ctx.fresh("arr.ptr");
    string elem_ptr = ctx.fresh("elem.ptr");
    string reg = ctx.fresh();
    ctx.out << "  " << arr_ptr << " = inttoptr i64 " << base << " to ptr\n";
    ctx.out << "  " << elem_ptr << " = getelementptr i64, ptr " << arr_ptr
            << ", i64 " << idx << "\n";
    ctx.out << "  " << reg << " = load i64, ptr " << elem_ptr << "\n";
    return reg;
}

string ArrayIndex::llvm_lval(LLVMGenCtx& ctx) {
    string base = _pointer->llvm_rval(ctx);
    string idx = _index->llvm_rval(ctx);
    string arr_ptr = ctx.fresh("arr.ptr");
    string elem_ptr = ctx.fresh("elem.ptr");
    ctx.out << "  " << arr_ptr << " = inttoptr i64 " << base << " to ptr\n";
    ctx.out << "  " << elem_ptr << " = getelementptr i64, ptr " << arr_ptr
            << ", i64 " << idx << "\n";
    return elem_ptr;
}

// ===== Address =====

string Address::llvm_rval(LLVMGenCtx& ctx) {
    string ptr = _exp->llvm_lval(ctx);
    string reg = ctx.fresh();
    ctx.out << "  " << reg << " = ptrtoint ptr " << ptr << " to i64\n";
    return reg;
}

// ===== RightSide =====

string RightSide::llvm_rval(LLVMGenCtx& ctx) { return "0"; }

// ===== Access =====

string Access::llvm_rval(LLVMGenCtx& ctx) {
    string addr = _rightside->llvm_rval(ctx);
    string ptr = ctx.fresh("ptr");
    string reg = ctx.fresh();
    ctx.out << "  " << ptr << " = inttoptr i64 " << addr << " to ptr\n";
    ctx.out << "  " << reg << " = load i64, ptr " << ptr << "\n";
    return reg;
}

string Access::llvm_lval(LLVMGenCtx& ctx) {
    string addr = _rightside->llvm_rval(ctx);
    string ptr = ctx.fresh("ptr");
    ctx.out << "  " << ptr << " = inttoptr i64 " << addr << " to ptr\n";
    return ptr;
}

// ===== CallFuncExp =====

string CallFuncExp::llvm_rval(LLVMGenCtx& ctx) {
    vector<string> arg_regs;
    for (auto arg : _args) {
        arg_regs.push_back(arg->llvm_rval(ctx));
    }
    string reg = ctx.fresh("call");
    bool is_imported = ctx.defined_funcs.find(_id) == ctx.defined_funcs.end();
    if (is_imported) {
        ctx.out << "  " << reg << " = call i64 (...) @" << _id << "(";
    } else {
        ctx.out << "  " << reg << " = call i64 @" << _id << "(";
    }
    for (int i = 0; i < (int)arg_regs.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << "i64 " << arg_regs[i];
    }
    ctx.out << ")\n";
    return reg;
}
