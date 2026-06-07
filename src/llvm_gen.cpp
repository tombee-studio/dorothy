/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <cstring>
#include "../include/ast.hpp"

using std::to_string;
using std::string;
using std::vector;

// Convert a value in canonical form to the declared target LLVM type.
static string llvm_coerce(LLVMGenCtx& ctx, const string& val,
                          VarType from_canonical, VarType to_declared) {
    if (from_canonical == VarType::DOUBLE && !is_float_type(to_declared)) {
        // double → integer
        string conv = ctx.fresh("conv");
        ctx.out << "  " << conv << " = fptosi double " << val
                << " to " << llvm_type_str(to_declared) << "\n";
        return conv;
    }
    if (from_canonical == VarType::LONG && is_float_type(to_declared)) {
        // integer → float/double
        string conv = ctx.fresh("conv");
        ctx.out << "  " << conv << " = sitofp i64 " << val
                << " to " << llvm_type_str(to_declared) << "\n";
        return conv;
    }
    if (from_canonical == VarType::DOUBLE && to_declared == VarType::FLOAT) {
        // double → float
        string conv = ctx.fresh("conv");
        ctx.out << "  " << conv << " = fptrunc double " << val << " to float\n";
        return conv;
    }
    if (from_canonical == VarType::LONG && to_declared == VarType::INT) {
        string conv = ctx.fresh("conv");
        ctx.out << "  " << conv << " = trunc i64 " << val << " to i32\n";
        return conv;
    }
    if (from_canonical == VarType::LONG && to_declared == VarType::CHAR) {
        string conv = ctx.fresh("conv");
        ctx.out << "  " << conv << " = trunc i64 " << val << " to i8\n";
        return conv;
    }
    return val;
}

// Ensure a value is in its canonical form (i64 for ints, double for floats).
static string llvm_to_canonical(LLVMGenCtx& ctx, const string& val,
                                VarType declared) {
    if (declared == VarType::CHAR) {
        string r = ctx.fresh("sext");
        ctx.out << "  " << r << " = sext i8 " << val << " to i64\n";
        return r;
    }
    if (declared == VarType::INT) {
        string r = ctx.fresh("sext");
        ctx.out << "  " << r << " = sext i32 " << val << " to i64\n";
        return r;
    }
    if (declared == VarType::FLOAT) {
        string r = ctx.fresh("fpext");
        ctx.out << "  " << r << " = fpext float " << val << " to double\n";
        return r;
    }
    return val;
}

// Promote a canonical value to double if needed (for mixed int+float arithmetic).
static string llvm_promote_to_double(LLVMGenCtx& ctx, const string& val,
                                     VarType canonical) {
    if (canonical == VarType::LONG) {
        string r = ctx.fresh("sitofp");
        ctx.out << "  " << r << " = sitofp i64 " << val << " to double\n";
        return r;
    }
    return val;
}

// ===== Expression base =====

string Expression::llvm_lval(LLVMGenCtx& ctx) {
    throw CompileError("expression cannot be used as left-value");
}

// ===== DeclVar =====

void DeclVar::llvm_emit(LLVMGenCtx& ctx) {
    int n = ctx.counter++;
    string tstr = llvm_type_str(_type);
    string ptr = "%" + _id + ".addr." + to_string(n);
    ctx.out << "  " << ptr << " = alloca " << tstr << "\n";
    if (is_float_type(_type)) {
        ctx.out << "  store " << tstr << " 0.0, ptr " << ptr << "\n";
    } else {
        ctx.out << "  store " << tstr << " 0, ptr " << ptr << "\n";
    }
    ctx.vars[_id] = ptr;
    ctx.var_types[_id] = _type;
    if (_is_const) ctx.const_vars.insert(_id);
}

// ===== InitializedDeclVar =====

void InitializedDeclVar::llvm_emit(LLVMGenCtx& ctx) {
    DeclVar::llvm_emit(ctx);  // alloca + zero-init + type/const tracking
    string val = _init->llvm_rval(ctx);
    VarType src_canonical = _init->llvm_etype(ctx);
    string tstr = llvm_type_str(_type);
    string store_val = llvm_coerce(ctx, val, src_canonical, _type);
    ctx.out << "  store " << tstr << " " << store_val << ", ptr "
            << ctx.vars[_id] << "\n";
}

void DeclVar::llvm_param(LLVMGenCtx& ctx, const string& param_reg) {
    int n = ctx.counter++;
    string tstr = llvm_type_str(_type);
    string ptr = "%" + _id + ".addr." + to_string(n);
    ctx.out << "  " << ptr << " = alloca " << tstr << "\n";
    ctx.out << "  store " << tstr << " " << param_reg << ", ptr " << ptr << "\n";
    ctx.vars[_id] = ptr;
    ctx.var_types[_id] = _type;
}

// ===== DeclArrayVar =====

void DeclArrayVar::llvm_emit(LLVMGenCtx& ctx) {
    int n = ctx.counter++;
    string tstr = llvm_type_str(_type);
    string data = "%" + _id + ".data." + to_string(n);
    string base = "%" + _id + ".base." + to_string(n);
    string ptr = "%" + _id + ".addr." + to_string(n);
    ctx.out << "  " << data << " = alloca " << tstr << ", i64 " << _num << "\n";
    ctx.out << "  " << base << " = ptrtoint ptr " << data << " to i64\n";
    ctx.out << "  " << ptr << " = alloca i64\n";
    ctx.out << "  store i64 " << base << ", ptr " << ptr << "\n";
    ctx.vars[_id] = ptr;
    ctx.var_types[_id] = VarType::LONG;  // pointer (address) is i64
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
    VarType etype = _exp->llvm_etype(ctx);
    // All non-main functions return i64; convert float if needed
    if (is_float_type(etype)) {
        string conv = ctx.fresh("ret.i64");
        ctx.out << "  " << conv << " = fptosi double " << val << " to i64\n";
        val = conv;
    }
    if (ctx.current_function == "main") {
        string trunc_reg = ctx.fresh("ret.i32");
        ctx.out << "  " << trunc_reg << " = trunc i64 " << val << " to i32\n";
        ctx.out << "  ret i32 " << trunc_reg << "\n";
    } else {
        ctx.out << "  ret i64 " << val << "\n";
    }
    ctx.terminated = true;
}

// Returns pairs of (llvm_type_str, reg) after emitting all prep instructions.
static std::vector<std::pair<string, string>>
prepareCallArgs(LLVMGenCtx& ctx, const std::string& id,
                const vector<Expression*>& args) {
    auto pit = ctx.func_param_types.find(id);
    std::vector<std::pair<string, string>> result;
    for (int i = 0; i < (int)args.size(); i++) {
        string val = args[i]->llvm_rval(ctx);
        VarType src_c = args[i]->llvm_etype(ctx);
        if (pit != ctx.func_param_types.end() && i < (int)pit->second.size()) {
            VarType tgt = pit->second[i];
            val = llvm_coerce(ctx, val, src_c, tgt);
            result.push_back({llvm_type_str(tgt), val});
        } else {
            result.push_back({"i64", val});
        }
    }
    return result;
}

// ===== CallFuncSt =====

void CallFuncSt::llvm_emit(LLVMGenCtx& ctx) {
    string reg = ctx.fresh("call");
    bool is_imported = ctx.defined_funcs.find(_id) == ctx.defined_funcs.end();
    // Prepare all args (emit conversions) before the call instruction
    auto prepared = prepareCallArgs(ctx, _id, _args);
    if (is_imported) {
        ctx.out << "  " << reg << " = call i64 (...) @" << _id << "(";
    } else {
        ctx.out << "  " << reg << " = call i64 @" << _id << "(";
    }
    for (int i = 0; i < (int)prepared.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << prepared[i].first << " " << prepared[i].second;
    }
    ctx.out << ")\n";
}

// ===== ExpressionSt =====

void ExpressionSt::llvm_emit(LLVMGenCtx& ctx) { _exp->llvm_rval(ctx); }

// ===== Function =====

void Function::llvm_emit(LLVMGenCtx& ctx) {
    ctx.vars.clear();
    ctx.var_types.clear();
    ctx.const_vars.clear();
    ctx.terminated = false;
    ctx.current_function = _id;

    bool is_main = (_id == "main");
    ctx.out << "define " << (is_main ? "i32" : "i64") << " @" << _id << "(";
    for (int i = 0; i < (int)_args.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << llvm_type_str(_args[i]->getType()) << " %param." << i;
    }
    ctx.out << ") {\nentry:\n";

    // Record parameter types for call sites
    std::vector<VarType> ptypes;
    for (auto arg : _args) ptypes.push_back(arg->getType());
    ctx.func_param_types[_id] = ptypes;

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
    const string& varname = _leftside->getVarName();
    if (!varname.empty() && ctx.const_vars.count(varname))
        throw CompileError(("cannot assign to constant: " + varname).c_str());
    string ptr = _leftside->llvm_lval(ctx);
    string val = _expr->llvm_rval(ctx);  // in canonical form
    VarType src_canonical = _expr->llvm_etype(ctx);
    VarType tgt_declared = _leftside->llvm_declared_type(ctx);
    string tstr = llvm_type_str(tgt_declared);
    string store_val = llvm_coerce(ctx, val, src_canonical, tgt_declared);
    ctx.out << "  store " << tstr << " " << store_val << ", ptr " << ptr << "\n";
    return val;  // return canonical source value for chained assignment
}

// ===== Binary arithmetic =====

static string emitBinOp(LLVMGenCtx& ctx, Expression* left, Expression* right,
                        const string& iop, const string& fop) {
    string l = left->llvm_rval(ctx);
    string r = right->llvm_rval(ctx);
    VarType lt = left->llvm_etype(ctx);
    VarType rt = right->llvm_etype(ctx);
    VarType et = promote_canonical(lt, rt);
    if (et == VarType::DOUBLE) {
        if (lt != VarType::DOUBLE) l = llvm_promote_to_double(ctx, l, lt);
        if (rt != VarType::DOUBLE) r = llvm_promote_to_double(ctx, r, rt);
        string reg = ctx.fresh();
        ctx.out << "  " << reg << " = " << fop << " double " << l << ", " << r << "\n";
        return reg;
    }
    string reg = ctx.fresh();
    ctx.out << "  " << reg << " = " << iop << " i64 " << l << ", " << r << "\n";
    return reg;
}

string AddExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitBinOp(ctx, _left, _right, "add", "fadd");
}

string SubExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitBinOp(ctx, _left, _right, "sub", "fsub");
}

string MulExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitBinOp(ctx, _left, _right, "mul", "fmul");
}

string DivExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitBinOp(ctx, _left, _right, "sdiv", "fdiv");
}

string ModExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitBinOp(ctx, _left, _right, "srem", "frem");
}

// ===== Comparison operators =====

static string emitCmp(LLVMGenCtx& ctx, Expression* left, Expression* right,
                      const string& ipred, const string& fpred) {
    string l = left->llvm_rval(ctx);
    string r = right->llvm_rval(ctx);
    VarType lt = left->llvm_etype(ctx);
    VarType rt = right->llvm_etype(ctx);
    VarType et = promote_canonical(lt, rt);
    string cmp = ctx.fresh("cmp");
    string reg = ctx.fresh();
    if (et == VarType::DOUBLE) {
        if (lt != VarType::DOUBLE) l = llvm_promote_to_double(ctx, l, lt);
        if (rt != VarType::DOUBLE) r = llvm_promote_to_double(ctx, r, rt);
        ctx.out << "  " << cmp << " = fcmp " << fpred << " double " << l << ", " << r << "\n";
    } else {
        ctx.out << "  " << cmp << " = icmp " << ipred << " i64 " << l << ", " << r << "\n";
    }
    ctx.out << "  " << reg << " = zext i1 " << cmp << " to i64\n";
    return reg;
}

string EQExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "eq",  "oeq");
}
string NEExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "ne",  "one");
}
string LTExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "slt", "olt");
}
string LEExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "sle", "ole");
}
string GTExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "sgt", "ogt");
}
string GEExp::llvm_rval(LLVMGenCtx& ctx) {
    return emitCmp(ctx, _left, _right, "sge", "oge");
}

// ===== IntExp =====

string IntExp::llvm_rval(LLVMGenCtx& ctx) { return to_string(_int_val); }

// ===== FloatExp =====

string FloatExp::llvm_rval(LLVMGenCtx& ctx) {
    // Emit as a double literal; LLVM requires hex float or decimal for doubles.
    string reg = ctx.fresh("flit");
    ctx.out << "  " << reg << " = fadd double 0.0, " << _float_val << "\n";
    return reg;
}

// ===== Variable =====

string Variable::llvm_rval(LLVMGenCtx& ctx) {
    auto it = ctx.vars.find(_id);
    if (it == ctx.vars.end()) {
        throw CompileError(("undefined variable: " + _id).c_str());
    }
    VarType declared = llvm_declared_type(ctx);
    string tstr = llvm_type_str(declared);
    string reg = ctx.fresh(_id);
    ctx.out << "  " << reg << " = load " << tstr << ", ptr " << it->second << "\n";
    return llvm_to_canonical(ctx, reg, declared);
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
    string reg = ctx.fresh("call");
    bool is_imported = ctx.defined_funcs.find(_id) == ctx.defined_funcs.end();
    // Prepare all args (emit conversions) before the call instruction
    auto prepared = prepareCallArgs(ctx, _id, _args);
    if (is_imported) {
        ctx.out << "  " << reg << " = call i64 (...) @" << _id << "(";
    } else {
        ctx.out << "  " << reg << " = call i64 @" << _id << "(";
    }
    for (int i = 0; i < (int)prepared.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << prepared[i].first << " " << prepared[i].second;
    }
    ctx.out << ")\n";
    return reg;
}
