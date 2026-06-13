/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <cstdio>
#include <cstring>
#include <map>
#include "../include/ast.hpp"

using std::to_string;
using std::string;
using std::vector;

// ===== C header import: type conversion and clang AST parsing =====

static string trim_str(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == string::npos) ? "" : s.substr(a, b - a + 1);
}

// Convert a C type string (e.g. "const char *restrict") to an LLVM type.
static string c_type_to_llvm(string t) {
    t = trim_str(t);
    // Pointer: any type containing '*' or '[]'
    if (t.find('*') != string::npos || t.find('[') != string::npos) return "ptr";
    // Remove qualifiers
    for (const char* q : {"const ", "volatile ", "restrict ", "__restrict__ ",
                           "__restrict ", "unsigned ", "signed "}) {
        size_t pos;
        string qw(q);
        while ((pos = t.find(qw)) != string::npos) t.erase(pos, qw.size());
    }
    t = trim_str(t);
    if (t == "void")   return "void";
    if (t == "_Bool" || t == "bool") return "i1";
    if (t == "char" || t == "signed char" || t == "unsigned char") return "i8";
    if (t == "short" || t == "short int") return "i16";
    if (t == "int" || t == "int32_t" || t == "uint32_t" || t == "wchar_t"
     || t == "__int32_t" || t == "__uint32_t") return "i32";
    if (t == "long" || t == "long int" || t == "long long" || t == "long long int"
     || t == "size_t" || t == "__SIZE_TYPE__" || t == "ssize_t" || t == "__SSIZE_TYPE__"
     || t == "ptrdiff_t" || t == "__PTRDIFF_TYPE__" || t == "intptr_t" || t == "uintptr_t"
     || t == "int64_t" || t == "uint64_t" || t == "__int64_t" || t == "__uint64_t"
     || t == "off_t" || t == "__off_t" || t == "__off64_t") return "i64";
    if (t == "float")  return "float";
    if (t == "double" || t == "long double") return "double";
    return "i64";  // fallback
}

// Parse "int (const char *, ...)" → {ret_llvm, [param_llvm_or_"..."]}
static std::pair<string, vector<string>> parse_c_func_type(const string& type_str) {
    size_t lp = type_str.find('(');
    if (lp == string::npos) return {"i64", {}};
    string ret_c = trim_str(type_str.substr(0, lp));
    string ret_llvm = c_type_to_llvm(ret_c);

    size_t rp = type_str.rfind(')');
    if (rp == string::npos || rp <= lp) return {ret_llvm, {}};
    string params_str = trim_str(type_str.substr(lp + 1, rp - lp - 1));

    vector<string> params;
    if (params_str.empty() || params_str == "void") return {ret_llvm, params};

    int depth = 0;
    string cur;
    for (char c : params_str) {
        if (c == '(' || c == '<') { depth++; cur += c; }
        else if (c == ')' || c == '>') { depth--; cur += c; }
        else if (c == ',' && depth == 0) {
            string p = trim_str(cur);
            params.push_back(p == "..." ? "..." : c_type_to_llvm(p));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        string p = trim_str(cur);
        params.push_back(p == "..." ? "..." : c_type_to_llvm(p));
    }
    return {ret_llvm, params};
}

// Strip ANSI escape codes from a string.
static string strip_ansi(const string& s) {
    string out;
    bool esc = false;
    for (char c : s) {
        if (c == '\033') { esc = true; continue; }
        if (esc) { if (c == 'm') esc = false; continue; }
        out += c;
    }
    return out;
}

// Run `clang -Xclang -ast-dump -fsyntax-only` on a C header and return
// a map of public function name → CImportedFunc.
static std::map<string, CImportedFunc> parse_c_header_funcs(const string& header_path) {
    std::map<string, CImportedFunc> result;

    // Write a small C file that includes the header
    const char* tmp_c = "/tmp/_dorothy_hdr_import.c";
    {
        FILE* f = fopen(tmp_c, "w");
        if (!f) return result;
        if (header_path.find('/') != string::npos || header_path[0] == '.')
            fprintf(f, "#include \"%s\"\n", header_path.c_str());
        else
            fprintf(f, "#include <%s>\n", header_path.c_str());
        fclose(f);
    }

    string cmd = "clang -Xclang -ast-dump -fsyntax-only " + string(tmp_c) + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { remove(tmp_c); return result; }

    char buf[8192];
    while (fgets(buf, sizeof(buf), pipe)) {
        string line = strip_ansi(string(buf));

        size_t fd_pos = line.find("FunctionDecl");
        if (fd_pos == string::npos) continue;

        // Find the type string in single quotes: 'int (const char *, ...)'
        size_t q1 = line.find('\'', fd_pos);
        if (q1 == string::npos) continue;
        size_t q2 = line.find('\'', q1 + 1);
        if (q2 == string::npos) continue;
        string type_str = line.substr(q1 + 1, q2 - q1 - 1);

        // Extract function name: last word before the opening quote
        string before_quote = line.substr(fd_pos, q1 - fd_pos);
        size_t name_end = before_quote.find_last_not_of(" \t");
        if (name_end == string::npos) continue;
        size_t name_start = before_quote.find_last_of(" \t", name_end);
        if (name_start == string::npos) continue;
        string func_name = before_quote.substr(name_start + 1, name_end - name_start);

        // Skip internal/compiler-private names
        if (func_name.empty() || func_name[0] == '_') continue;
        // Must look like a C identifier
        bool valid = true;
        for (char c : func_name)
            if (!isalnum(c) && c != '_') { valid = false; break; }
        if (!valid) continue;

        // Only add the first (most general) declaration for each name
        if (result.count(func_name)) continue;

        auto [ret_llvm, params] = parse_c_func_type(type_str);
        CImportedFunc info;
        info.ret_type = ret_llvm;
        info.is_variadic = false;
        for (const auto& p : params) {
            if (p == "...") info.is_variadic = true;
            else            info.param_types.push_back(p);
        }
        result[func_name] = info;
    }

    pclose(pipe);
    remove(tmp_c);
    return result;
}

// Emit a call to a C-imported function using the raw Expression objects.
// Applies C default argument promotions for variadic arguments.
// Returns the i64 result register (or "0" for void).
static string emitCImportedCall(LLVMGenCtx& ctx, const string& id,
                                const CImportedFunc& info,
                                const vector<Expression*>& args) {
    vector<std::pair<string, string>> final_args;

    for (int i = 0; i < (int)args.size(); i++) {
        string val = args[i]->llvm_rval(ctx);      // canonical form (i64 or double)
        VarType canonical = args[i]->llvm_etype(ctx);

        if (i < (int)info.param_types.size()) {
            // Known parameter: convert to the declared C type
            const string& ptype = info.param_types[i];
            if (ptype == "ptr") {
                // Prefer the raw alloca ptr for array variables (better provenance)
                const string& varname = args[i]->getVarName();
                auto it = ctx.array_data_ptrs.find(varname);
                if (it != ctx.array_data_ptrs.end()) {
                    final_args.push_back({"ptr", it->second});
                } else {
                    string r = ctx.fresh("arg.ptr");
                    ctx.out << "  " << r << " = inttoptr i64 " << val << " to ptr\n";
                    final_args.push_back({"ptr", r});
                }
            } else if (ptype == "i32") {
                string r = ctx.fresh("arg.i32");
                ctx.out << "  " << r << " = trunc i64 " << val << " to i32\n";
                final_args.push_back({"i32", r});
            } else {
                final_args.push_back({ptype, val});
            }
        } else {
            // Variadic argument: apply C default argument promotions.
            //   float  → double (already double in canonical form)
            //   char/int/short → int (i32)
            //   long   → i64
            //   address-of (&var) → ptr (use raw alloca when available)
            if (canonical == VarType::DOUBLE) {
                final_args.push_back({"double", val});
            } else {
                // Check if this is an address-of expression pointing to an array
                // or a scalar variable: prefer passing as ptr for better provenance
                auto* addr_expr = dynamic_cast<Address*>(args[i]);
                if (addr_expr) {
                    // &expr: pass as ptr using llvm_lval for provenance safety
                    string ptr_reg = addr_expr->llvm_ptr(ctx);
                    if (!ptr_reg.empty()) {
                        final_args.push_back({"ptr", ptr_reg});
                    } else {
                        // Fallback: inttoptr (loses provenance)
                        string r = ctx.fresh("va.ptr");
                        ctx.out << "  " << r << " = inttoptr i64 " << val << " to ptr\n";
                        final_args.push_back({"ptr", r});
                    }
                } else {
                    VarType declared = args[i]->llvm_declared_type(ctx);
                    if (declared == VarType::LONG) {
                        // Explicitly long — keep as i64
                        final_args.push_back({"i64", val});
                    } else {
                        // char/int/literal integer → promote to i32
                        string r = ctx.fresh("va.i32");
                        ctx.out << "  " << r << " = trunc i64 " << val << " to i32\n";
                        final_args.push_back({"i32", r});
                    }
                }
            }
        }
    }

    const string& ret = info.ret_type;
    string tmp = (ret != "void") ? ctx.fresh("call.c") : "";

    // Build the explicit function type annotation required for correct AArch64 ABI.
    // Without it, variadic calls misbehave on macOS ARM64.
    string fn_type = ret + " (";
    for (int i = 0; i < (int)info.param_types.size(); i++) {
        if (i > 0) fn_type += ", ";
        fn_type += info.param_types[i];
    }
    if (info.is_variadic) {
        if (!info.param_types.empty()) fn_type += ", ";
        fn_type += "...";
    }
    fn_type += ")";

    ctx.out << "  ";
    if (ret != "void") ctx.out << tmp << " = ";
    ctx.out << "call " << fn_type << " @" << id << "(";
    for (int i = 0; i < (int)final_args.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << final_args[i].first << " " << final_args[i].second;
    }
    ctx.out << ")\n";

    if (ret == "void")   return "0";
    if (ret == "i32") {
        string r = ctx.fresh("sext");
        ctx.out << "  " << r << " = sext i32 " << tmp << " to i64\n";
        return r;
    }
    if (ret == "ptr") {
        string r = ctx.fresh("ptrtoint");
        ctx.out << "  " << r << " = ptrtoint ptr " << tmp << " to i64\n";
        return r;
    }
    if (ret == "float" || ret == "double") {
        string r = ctx.fresh("fptosi");
        ctx.out << "  " << r << " = fptosi " << ret << " " << tmp << " to i64\n";
        return r;
    }
    return tmp;  // i64
}

static std::vector<std::pair<string, string>>
prepareCallArgs(LLVMGenCtx&, const string&, const vector<Expression*>&);

static string resolve_struct_var(LLVMGenCtx&, const string&);

// ===== Nested-struct helpers =====

struct LeafField { string path; VarType type; };

// Recursively enumerate all primitive leaf fields of a struct with dotted paths.
static vector<LeafField> get_leaf_fields(const string& struct_name, const string& prefix = "") {
    vector<LeafField> result;
    const auto& sdef = g_struct_defs[struct_name];
    for (auto& field : sdef.fields) {
        string full = prefix.empty() ? field.name : prefix + "." + field.name;
        if (field.type == VarType::STRUCT) {
            for (auto& sub : get_leaf_fields(field.struct_name, full))
                result.push_back(sub);
        } else {
            result.push_back({full, field.type});
        }
    }
    return result;
}

// Resolve the VarType at a dotted path within a struct hierarchy.
static VarType resolve_path_field_type(const string& struct_name, const string& path) {
    size_t dot = path.find('.');
    const auto& sdef = g_struct_defs[struct_name];
    if (dot == string::npos) {
        int idx = sdef.fieldIndex(path);
        if (idx < 0) throw CompileError(("no field '" + path + "' in " + struct_name).c_str());
        return sdef.fields[idx].type;
    }
    string head = path.substr(0, dot);
    string tail = path.substr(dot + 1);
    int idx = sdef.fieldIndex(head);
    if (idx < 0) throw CompileError(("no field '" + head + "' in " + struct_name).c_str());
    if (sdef.fields[idx].type != VarType::STRUCT)
        throw CompileError(("field '" + head + "' is not a struct").c_str());
    return resolve_path_field_type(sdef.fields[idx].struct_name, tail);
}

// Resolve the struct_name at a dotted path (for non-leaf struct fields).
static string resolve_path_struct_name(const string& struct_name, const string& path) {
    size_t dot = path.find('.');
    const auto& sdef = g_struct_defs[struct_name];
    string head = (dot == string::npos) ? path : path.substr(0, dot);
    string tail = (dot == string::npos) ? "" : path.substr(dot + 1);
    int idx = sdef.fieldIndex(head);
    if (idx < 0) throw CompileError(("no field '" + head + "' in " + struct_name).c_str());
    if (sdef.fields[idx].type != VarType::STRUCT)
        throw CompileError(("field '" + head + "' is not a struct type").c_str());
    if (tail.empty()) return sdef.fields[idx].struct_name;
    return resolve_path_struct_name(sdef.fields[idx].struct_name, tail);
}

// Allocate all leaf allocas for a struct variable (recursive).
static void alloc_struct_fields(LLVMGenCtx& ctx, const string& var_id,
                                 const string& struct_name, const string& prefix) {
    const auto& sdef = g_struct_defs[struct_name];
    for (auto& field : sdef.fields) {
        string full = prefix.empty() ? field.name : prefix + "." + field.name;
        if (field.type == VarType::STRUCT) {
            ctx.struct_subfield_types[var_id][full] = field.struct_name;
            alloc_struct_fields(ctx, var_id, field.struct_name, full);
        } else {
            int n = ctx.counter++;
            string tstr = llvm_type_str(field.type);
            string ptr = "%" + var_id + "." + full + ".addr." + to_string(n);
            ctx.out << "  " << ptr << " = alloca " << tstr << "\n";
            if (is_float_type(field.type))
                ctx.out << "  store " << tstr << " 0.0, ptr " << ptr << "\n";
            else
                ctx.out << "  store " << tstr << " 0, ptr " << ptr << "\n";
            ctx.struct_field_ptrs[var_id][full] = ptr;
        }
    }
}

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
    if (_type == VarType::STRUCT) {
        if (!g_struct_defs.count(_struct_name))
            throw CompileError(("undefined struct type: " + _struct_name).c_str());
        ctx.struct_var_types[_id] = _struct_name;
        if (_is_const) ctx.const_vars.insert(_id);
        alloc_struct_fields(ctx, _id, _struct_name, "");
        return;
    }
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

    if (_type == VarType::STRUCT) {
        // struct-returning function call: call void @func(ptr %sret..., args...)
        auto cfe = dynamic_cast<CallFuncExp*>(_init);
        if (cfe && ctx.func_return_struct.count(cfe->getId())) {
            bool is_imported = ctx.defined_funcs.find(cfe->getId()) == ctx.defined_funcs.end();
            vector<pair<string, string>> all_args;
            for (auto& leaf : get_leaf_fields(_struct_name))
                all_args.push_back({"ptr", ctx.struct_field_ptrs[_id][leaf.path]});
            for (auto& p : prepareCallArgs(ctx, cfe->getId(), cfe->getArgs()))
                all_args.push_back(p);
            ctx.out << "  call void ";
            if (is_imported) ctx.out << "(...) ";
            ctx.out << "@" << cfe->getId() << "(";
            for (int i = 0; i < (int)all_args.size(); i++) {
                if (i > 0) ctx.out << ", ";
                ctx.out << all_args[i].first << " " << all_args[i].second;
            }
            ctx.out << ")\n";
            return;
        }

        // copy from nested struct sub-field: var copy: Inner = outer.a;
        auto ma_src = dynamic_cast<MemberAccess*>(_init);
        if (ma_src) {
            string root_var = resolve_struct_var(ctx, ma_src->getVarName());
            string src_path = ma_src->getFieldPath();
            if (!root_var.empty() && ctx.struct_subfield_types.count(root_var) &&
                ctx.struct_subfield_types[root_var].count(src_path)) {
                const string& src_struct = ctx.struct_subfield_types[root_var][src_path];
                if (src_struct != _struct_name)
                    throw CompileError(("cannot copy struct '" + src_struct +
                                        "' into variable of type '" + _struct_name + "'").c_str());
                for (auto& leaf : get_leaf_fields(_struct_name)) {
                    string tstr = llvm_type_str(leaf.type);
                    string src_ptr = ctx.struct_field_ptrs[root_var][src_path + "." + leaf.path];
                    string dst_ptr = ctx.struct_field_ptrs[_id][leaf.path];
                    string val = ctx.fresh("copy.field");
                    ctx.out << "  " << val << " = load " << tstr << ", ptr " << src_ptr << "\n";
                    ctx.out << "  store " << tstr << " " << val << ", ptr " << dst_ptr << "\n";
                }
                return;
            }
        }

        // struct-to-struct copy: var a: P = b;
        const string& raw_src = _init->getVarName();
        string src_var = (raw_src == "$this" && !ctx.this_var.empty()) ? ctx.this_var : raw_src;
        if (!src_var.empty() && ctx.struct_var_types.count(src_var)) {
            const string& src_struct = ctx.struct_var_types[src_var];
            if (src_struct != _struct_name)
                throw CompileError(("cannot copy struct '" + src_struct +
                                    "' into variable of type '" + _struct_name + "'").c_str());
            for (auto& leaf : get_leaf_fields(_struct_name)) {
                string tstr = llvm_type_str(leaf.type);
                string src_ptr = ctx.struct_field_ptrs[src_var][leaf.path];
                string dst_ptr = ctx.struct_field_ptrs[_id][leaf.path];
                string val = ctx.fresh("copy.field");
                ctx.out << "  " << val << " = load " << tstr << ", ptr " << src_ptr << "\n";
                ctx.out << "  store " << tstr << " " << val << ", ptr " << dst_ptr << "\n";
            }
            return;
        }

        auto si = dynamic_cast<StructInit*>(_init);
        if (!si)
            throw CompileError("struct variable must be initialized with struct literal, struct-returning function, or another struct variable");
        const auto& sdef = g_struct_defs[_struct_name];

        // Save and set 'this' context
        string prev_this_var = ctx.this_var;
        ctx.this_var = _id;

        // Execute constructor: allocate param allocas, assign values, compile body
        if (sdef.constructor) {
            const auto& call_args = si->getArgs();
            for (int pi = 0; pi < (int)sdef.constructor->params.size(); pi++) {
                const string& pname = sdef.constructor->params[pi].first;
                VarType ptype = sdef.constructor->params[pi].second;
                int n = ctx.counter++;
                string tstr = llvm_type_str(ptype);
                string ptr = "%" + pname + ".param." + to_string(n);
                ctx.out << "  " << ptr << " = alloca " << tstr << "\n";
                if (is_float_type(ptype)) {
                    ctx.out << "  store " << tstr << " 0.0, ptr " << ptr << "\n";
                } else {
                    ctx.out << "  store " << tstr << " 0, ptr " << ptr << "\n";
                }
                ctx.vars[pname] = ptr;
                ctx.var_types[pname] = ptype;

                // Store positional arg value
                if (pi < (int)call_args.size()) {
                    string val = call_args[pi]->llvm_rval(ctx);
                    VarType src_c = call_args[pi]->llvm_etype(ctx);
                    string store_val = llvm_coerce(ctx, val, src_c, ptype);
                    ctx.out << "  store " << tstr << " " << store_val
                            << ", ptr " << ptr << "\n";
                }
            }
            sdef.constructor->body->llvm_emit(ctx);

            // Remove param vars from context
            for (auto& [pname, ptype] : sdef.constructor->params) {
                ctx.vars.erase(pname);
                ctx.var_types.erase(pname);
            }
        }

        ctx.this_var = prev_this_var;
        return;
    }

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
    // Use typed array alloca [N x T] for correct AArch64 ABI handling.
    ctx.out << "  " << data << " = alloca [" << _num << " x " << tstr << "]\n";
    ctx.out << "  " << base << " = ptrtoint ptr " << data << " to i64\n";
    ctx.out << "  " << ptr << " = alloca i64\n";
    ctx.out << "  store i64 " << base << ", ptr " << ptr << "\n";
    ctx.vars[_id] = ptr;
    ctx.var_types[_id] = VarType::LONG;  // pointer (address) is i64
    ctx.array_data_ptrs[_id] = data;     // raw alloca ptr for provenance-safe access
}

// ===== InitializedDeclArrayVar =====

void InitializedDeclArrayVar::llvm_emit(LLVMGenCtx& ctx) {
    DeclArrayVar::llvm_emit(ctx);

    // Use the raw alloca pointer directly to preserve LLVM pointer provenance.
    string arr_ptr = ctx.array_data_ptrs[_id];

    string tstr = llvm_type_str(_type);
    for (int i = 0; i < (int)_values.size(); i++) {
        string val = _values[i]->llvm_rval(ctx);
        string elem = ctx.fresh("elem");
        ctx.out << "  " << elem << " = getelementptr " << tstr << ", ptr " << arr_ptr
                << ", i64 " << i << "\n";
        string store_val = llvm_coerce(ctx, val, VarType::LONG, _type);
        ctx.out << "  store " << tstr << " " << store_val << ", ptr " << elem << "\n";
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
    // Struct return: copy each field to the caller's sret pointers
    if (!ctx.current_ret_struct.empty()) {
        const string& varname = _exp->getVarName();
        if (varname.empty() || !ctx.struct_var_types.count(varname))
            throw CompileError("return value must be a struct variable");
        const string& var_struct = ctx.struct_var_types[varname];
        if (var_struct != ctx.current_ret_struct)
            throw CompileError(("cannot return struct '" + var_struct +
                                "' from function expecting '" + ctx.current_ret_struct + "'").c_str());
        auto leaves = get_leaf_fields(ctx.current_ret_struct);
        for (int fi = 0; fi < (int)leaves.size(); fi++) {
            string tstr = llvm_type_str(leaves[fi].type);
            string ptr = ctx.struct_field_ptrs[varname][leaves[fi].path];
            string val = ctx.fresh("ret.field");
            ctx.out << "  " << val << " = load " << tstr << ", ptr " << ptr << "\n";
            ctx.out << "  store " << tstr << " " << val
                    << ", ptr " << ctx.sret_field_ptrs[fi] << "\n";
        }
        ctx.out << "  ret void\n";
        ctx.terminated = true;
        return;
    }

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
// Struct arguments are expanded to individual field values.
static std::vector<std::pair<string, string>>
prepareCallArgs(LLVMGenCtx& ctx, const std::string& id,
                const vector<Expression*>& args) {
    auto pit = ctx.func_param_info.find(id);
    std::vector<std::pair<string, string>> result;
    for (int i = 0; i < (int)args.size(); i++) {
        bool param_is_struct = (pit != ctx.func_param_info.end() &&
                                i < (int)pit->second.size() &&
                                pit->second[i].type == VarType::STRUCT);
        if (param_is_struct) {
            const string& struct_name = pit->second[i].struct_name;
            const string& varname = args[i]->getVarName();
            if (varname.empty() || !ctx.struct_var_types.count(varname))
                throw CompileError("expected struct variable for struct parameter");
            for (auto& leaf : get_leaf_fields(struct_name)) {
                string tstr = llvm_type_str(leaf.type);
                string ptr = ctx.struct_field_ptrs[varname][leaf.path];
                string val = ctx.fresh("arg.field");
                ctx.out << "  " << val << " = load " << tstr << ", ptr " << ptr << "\n";
                result.push_back({tstr, val});
            }
        } else {
            string val = args[i]->llvm_rval(ctx);
            VarType src_c = args[i]->llvm_etype(ctx);
            if (pit != ctx.func_param_info.end() && i < (int)pit->second.size()) {
                VarType tgt = pit->second[i].type;
                val = llvm_coerce(ctx, val, src_c, tgt);
                result.push_back({llvm_type_str(tgt), val});
            } else {
                result.push_back({"i64", val});
            }
        }
    }
    return result;
}

// ===== CallFuncSt =====

void CallFuncSt::llvm_emit(LLVMGenCtx& ctx) {
    if (ctx.func_return_struct.count(_id))
        throw CompileError(("struct-returning function result must be assigned to a struct variable: " + _id).c_str());
    // C-imported function: use correct signature with C arg promotions
    auto cit = ctx.c_imported_funcs.find(_id);
    if (cit != ctx.c_imported_funcs.end()) {
        emitCImportedCall(ctx, _id, cit->second, _args);
        return;
    }
    // Prepare all args (emit conversions) before the call instruction
    auto prepared = prepareCallArgs(ctx, _id, _args);
    string reg = ctx.fresh("call");
    bool is_imported = ctx.defined_funcs.find(_id) == ctx.defined_funcs.end();
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

void Function::llvm_pre_register(LLVMGenCtx& ctx) {
    if (!_ret_struct_name.empty())
        ctx.func_return_struct[_id] = _ret_struct_name;
    vector<ParamInfo> pinfo;
    vector<VarType> flat_types;
    for (auto* arg : _args) {
        pinfo.push_back({arg->getType(), arg->getStructName()});
        if (arg->getType() == VarType::STRUCT) {
            for (auto& leaf : get_leaf_fields(arg->getStructName()))
                flat_types.push_back(leaf.type);
        } else {
            flat_types.push_back(arg->getType());
        }
    }
    ctx.func_param_info[_id] = pinfo;
    ctx.func_param_types[_id] = flat_types;
}

void Function::llvm_emit(LLVMGenCtx& ctx) {
    ctx.vars.clear();
    ctx.var_types.clear();
    ctx.const_vars.clear();
    ctx.struct_field_ptrs.clear();
    ctx.struct_var_types.clear();
    ctx.struct_subfield_types.clear();
    ctx.this_var.clear();
    ctx.sret_field_ptrs.clear();
    ctx.current_ret_struct = _ret_struct_name;
    ctx.terminated = false;
    ctx.current_function = _id;

    bool is_main = (_id == "main");
    bool returns_struct = !_ret_struct_name.empty();

    // Register signatures (also done by pre_register, but ensure consistency)
    llvm_pre_register(ctx);

    string ret_llvm = is_main ? "i32" : (returns_struct ? "void" : "i64");

    // Build LLVM param list: [sret ptrs...] [expanded field params...]
    vector<pair<string, string>> llvm_params;

    if (returns_struct) {
        auto ret_leaves = get_leaf_fields(_ret_struct_name);
        for (int fi = 0; fi < (int)ret_leaves.size(); fi++) {
            string reg = "%sret." + to_string(fi);
            llvm_params.push_back({"ptr", reg});
            ctx.sret_field_ptrs.push_back(reg);
        }
    }

    int flat_idx = 0;
    for (auto* arg : _args) {
        if (arg->getType() == VarType::STRUCT) {
            for (auto& leaf : get_leaf_fields(arg->getStructName()))
                llvm_params.push_back({llvm_type_str(leaf.type),
                                       "%param." + to_string(flat_idx++)});
        } else {
            llvm_params.push_back({llvm_type_str(arg->getType()),
                                   "%param." + to_string(flat_idx++)});
        }
    }

    ctx.out << "define " << ret_llvm << " @" << _id << "(";
    for (int i = 0; i < (int)llvm_params.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << llvm_params[i].first << " " << llvm_params[i].second;
    }
    ctx.out << ") {\nentry:\n";

    // Allocate and initialize parameters
    flat_idx = 0;
    for (auto* arg : _args) {
        if (arg->getType() == VarType::STRUCT) {
            const string& sname = arg->getStructName();
            const string& pname = arg->getId();
            ctx.struct_var_types[pname] = sname;
            if (arg->isConst()) ctx.const_vars.insert(pname);
            // Allocate all leaf fields (recursive) and populate subfield types
            alloc_struct_fields(ctx, pname, sname, "");
            // Overwrite zero-initialized values with parameter values
            for (auto& leaf : get_leaf_fields(sname)) {
                string tstr = llvm_type_str(leaf.type);
                string ptr = ctx.struct_field_ptrs[pname][leaf.path];
                string flat_reg = "%param." + to_string(flat_idx++);
                ctx.out << "  store " << tstr << " " << flat_reg << ", ptr " << ptr << "\n";
            }
        } else {
            arg->llvm_param(ctx, "%param." + to_string(flat_idx++));
        }
    }

    _body->llvm_emit(ctx);

    if (!ctx.terminated) {
        if (returns_struct)
            ctx.out << "  ret void\n";
        else
            ctx.out << (is_main ? "  ret i32 0\n" : "  ret i64 0\n");
    }

    ctx.out << "}\n\n";
}

// ===== ImportCHeader =====

void ImportCHeader::llvm_emit(LLVMGenCtx& ctx) {
    auto funcs = parse_c_header_funcs(_header_path);
    for (auto& [name, info] : funcs) {
        ctx.c_imported_funcs[name] = info;
        ctx.out << "declare " << info.ret_type << " @" << name << "(";
        for (int i = 0; i < (int)info.param_types.size(); i++) {
            if (i > 0) ctx.out << ", ";
            ctx.out << info.param_types[i];
        }
        if (info.is_variadic) {
            if (!info.param_types.empty()) ctx.out << ", ";
            ctx.out << "...";
        }
        ctx.out << ")\n";
    }
    ctx.out << "\n";
}

// ===== ImportFunction =====

void ImportFunction::llvm_emit(LLVMGenCtx& ctx) {
    // Skip if already declared via a C header import
    if (ctx.c_imported_funcs.count(_id)) return;
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

// ===== MemberAccess =====

static string resolve_struct_var(LLVMGenCtx& ctx, const string& varname) {
    // "$this" resolves to the actual struct variable name
    return (varname == "$this" && !ctx.this_var.empty()) ? ctx.this_var : varname;
}

static VarType member_field_type(LLVMGenCtx& ctx, const string& varname,
                                  const string& path) {
    string resolved = resolve_struct_var(ctx, varname);
    auto sit = ctx.struct_var_types.find(resolved);
    if (sit == ctx.struct_var_types.end())
        throw CompileError(("not a struct variable: " + resolved).c_str());
    return resolve_path_field_type(sit->second, path);
}

string MemberAccess::llvm_rval(LLVMGenCtx& ctx) {
    string varname = resolve_struct_var(ctx, _object->getVarName());
    string path = getFieldPath();
    string ptr = ctx.struct_field_ptrs[varname][path];
    if (ptr.empty())
        throw CompileError(("struct field '" + path + "' is a struct type and cannot be used as a primitive value").c_str());
    VarType ftype = member_field_type(ctx, varname, path);
    string tstr = llvm_type_str(ftype);
    string reg = ctx.fresh(varname + "_" + path);
    ctx.out << "  " << reg << " = load " << tstr << ", ptr " << ptr << "\n";
    return llvm_to_canonical(ctx, reg, ftype);
}

string MemberAccess::llvm_lval(LLVMGenCtx& ctx) {
    string varname = resolve_struct_var(ctx, _object->getVarName());
    string path = getFieldPath();
    string ptr = ctx.struct_field_ptrs[varname][path];
    if (ptr.empty())
        throw CompileError(("struct field '" + path + "' is a struct type; assign field-by-field").c_str());
    return ptr;
}

VarType MemberAccess::llvm_etype(LLVMGenCtx& ctx) const {
    string varname = resolve_struct_var(ctx, _object->getVarName());
    return canonical_type(member_field_type(ctx, varname, getFieldPath()));
}

VarType MemberAccess::llvm_declared_type(LLVMGenCtx& ctx) const {
    string varname = resolve_struct_var(ctx, _object->getVarName());
    return member_field_type(ctx, varname, getFieldPath());
}

// ===== ThisExpr =====

string ThisExpr::llvm_rval(LLVMGenCtx& ctx) {
    throw CompileError("'this' cannot be used as a standalone rvalue");
}

string ThisExpr::llvm_lval(LLVMGenCtx& ctx) {
    throw CompileError("'this' cannot be used as a standalone lvalue");
}

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
        if (ctx.struct_var_types.count(_id))
            throw CompileError(("struct variable cannot be used as a value: " + _id).c_str());
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
        if (ctx.struct_var_types.count(_id))
            throw CompileError(("struct variable cannot be used as a value: " + _id).c_str());
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
    if (ctx.func_return_struct.count(_id))
        throw CompileError(("struct-returning function result must be assigned to a struct variable: " + _id).c_str());
    // C-imported function: use correct signature with C arg promotions
    auto cit = ctx.c_imported_funcs.find(_id);
    if (cit != ctx.c_imported_funcs.end())
        return emitCImportedCall(ctx, _id, cit->second, _args);
    // Prepare all args (emit conversions) before the call instruction
    auto prepared = prepareCallArgs(ctx, _id, _args);
    string reg = ctx.fresh("call");
    bool is_imported = ctx.defined_funcs.find(_id) == ctx.defined_funcs.end();
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
