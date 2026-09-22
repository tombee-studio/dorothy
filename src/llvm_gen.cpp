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
                           "__restrict ", "unsigned ", "signed ", "struct ", "enum ", "union "}) {
        size_t pos;
        string qw(q);
        while ((pos = t.find(qw)) != string::npos) t.erase(pos, qw.size());
    }
    t = trim_str(t);
    if (t == "void")   return "void";
    if (t == "_Bool" || t == "bool") return "i1";
    if (t == "char" || t == "Uint8" || t == "Sint8" || t == "uint8_t" || t == "int8_t"
     || t == "int8" || t == "uint8") return "i8";
    if (t == "short" || t == "short int" || t == "Uint16" || t == "Sint16"
     || t == "uint16_t" || t == "int16_t" || t == "int16" || t == "uint16") return "i16";
    if (t == "int" || t == "int32_t" || t == "uint32_t" || t == "Uint32" || t == "Sint32"
     || t == "int32" || t == "uint32" || t == "wchar_t"
     || t == "__int32_t" || t == "__uint32_t") return "i32";
    if (t == "long" || t == "long int" || t == "long long" || t == "long long int"
     || t == "Uint64" || t == "Sint64" || t == "int64_t" || t == "uint64_t"
     || t == "int64" || t == "uint64"
     || t == "size_t" || t == "__SIZE_TYPE__" || t == "ssize_t" || t == "__SSIZE_TYPE__"
     || t == "ptrdiff_t" || t == "__PTRDIFF_TYPE__" || t == "intptr_t" || t == "uintptr_t"
     || t == "off_t" || t == "__off_t" || t == "__off64_t") return "i64";
    if (t == "float")  return "float";
    if (t == "double" || t == "long double") return "double";
    if (t.rfind("SDL_", 0) == 0) return "i32";  // SDL enums and scalar typedefs
    return "i32";  // default unknown C scalar/enum types to i32
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

// Escape raw characters for LLVM IR string literal constant
static string llvm_escape_string(const string& s) {
    string out;
    for (unsigned char c : s) {
        if (c >= 32 && c <= 126 && c != '\\' && c != '"') {
            out += c;
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\%02X", c);
            out += buf;
        }
    }
    return out;
}

static void emit_string_runtime(LLVMGenCtx& ctx) {
    if (ctx.str_runtime_emitted) return;
    ctx.str_runtime_emitted = true;

    if (!ctx.c_imported_funcs.count("malloc")) {
        ctx.out << "declare ptr @malloc(i64)\n\n";
        ctx.c_imported_funcs["malloc"] = {"ptr", {"i64"}, false};
    }
    if (!ctx.c_imported_funcs.count("strlen")) {
        ctx.out << "declare i64 @strlen(ptr)\n\n";
        ctx.c_imported_funcs["strlen"] = {"i64", {"ptr"}, false};
    }
    if (!ctx.c_imported_funcs.count("strcpy")) {
        ctx.out << "declare ptr @strcpy(ptr, ptr)\n\n";
        ctx.c_imported_funcs["strcpy"] = {"ptr", {"ptr", "ptr"}, false};
    }
    if (!ctx.c_imported_funcs.count("strcat")) {
        ctx.out << "declare ptr @strcat(ptr, ptr)\n\n";
        ctx.c_imported_funcs["strcat"] = {"ptr", {"ptr", "ptr"}, false};
    }
    if (!ctx.c_imported_funcs.count("strcmp")) {
        ctx.out << "declare i32 @strcmp(ptr, ptr)\n\n";
        ctx.c_imported_funcs["strcmp"] = {"i32", {"ptr", "ptr"}, false};
    }

    ctx.out << "@.str.empty = private unnamed_addr constant [1 x i8] c\"\\00\", align 1\n\n";

    ctx.out << "define ptr @_dorothy_str_concat(ptr %s1, ptr %s2) {\n"
            << "entry:\n"
            << "  %s1_null = icmp eq ptr %s1, null\n"
            << "  %s1_safe = select i1 %s1_null, ptr @.str.empty, ptr %s1\n"
            << "  %s2_null = icmp eq ptr %s2, null\n"
            << "  %s2_safe = select i1 %s2_null, ptr @.str.empty, ptr %s2\n"
            << "  %l1 = call i64 @strlen(ptr %s1_safe)\n"
            << "  %l2 = call i64 @strlen(ptr %s2_safe)\n"
            << "  %tot = add i64 %l1, %l2\n"
            << "  %tot1 = add i64 %tot, 1\n"
            << "  %mem = call ptr @malloc(i64 %tot1)\n"
            << "  %c1 = call ptr @strcpy(ptr %mem, ptr %s1_safe)\n"
            << "  %c2 = call ptr @strcat(ptr %mem, ptr %s2_safe)\n"
            << "  ret ptr %mem\n"
            << "}\n\n";

    ctx.out << "define i32 @_dorothy_str_cmp(ptr %s1, ptr %s2) {\n"
            << "entry:\n"
            << "  %s1_null = icmp eq ptr %s1, null\n"
            << "  %s1_safe = select i1 %s1_null, ptr @.str.empty, ptr %s1\n"
            << "  %s2_null = icmp eq ptr %s2, null\n"
            << "  %s2_safe = select i1 %s2_null, ptr @.str.empty, ptr %s2\n"
            << "  %res = call i32 @strcmp(ptr %s1_safe, ptr %s2_safe)\n"
            << "  ret i32 %res\n"
            << "}\n\n";
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
        if (header_path.rfind("./", 0) == 0 || header_path.rfind("../", 0) == 0 || header_path[0] == '/')
            fprintf(f, "#include \"%s\"\n", header_path.c_str());
        else
            fprintf(f, "#include <%s>\n", header_path.c_str());
        fclose(f);
    }

    string cmd = "clang -Xclang -ast-dump -fsyntax-only -I. -I./include -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -I/usr/local/include -I/usr/local/include/SDL2 -I/usr/include " + string(tmp_c) + " 2>/dev/null";
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

        auto parsed_sig = parse_c_func_type(type_str);
        CImportedFunc info;
        info.ret_type = parsed_sig.first;
        info.is_variadic = false;
        for (const auto& p : parsed_sig.second) {
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
            } else if (ptype == "i8" || ptype == "i16" || ptype == "i32") {
                string r = ctx.fresh("arg." + ptype);
                ctx.out << "  " << r << " = trunc i64 " << val << " to " << ptype << "\n";
                final_args.push_back({ptype, r});
            } else if (ptype == "float") {
                string r = ctx.fresh("arg.float");
                ctx.out << "  " << r << " = fptrunc double " << val << " to float\n";
                final_args.push_back({ptype, r});
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
                    if (declared == VarType::STRING || dynamic_cast<StringExp*>(args[i])) {
                        string r = ctx.fresh("va.ptr");
                        ctx.out << "  " << r << " = inttoptr i64 " << val << " to ptr\n";
                        final_args.push_back({"ptr", r});
                    } else if (declared == VarType::LONG) {
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

// ===== Scope & ARC helper implementations =====

void LLVMGenCtx::emit_release_scope(const LLVMGenCtx::Scope& scope) {
    for (const auto& addr : scope.class_var_ptrs) {
        string loaded = fresh("rel.var");
        out << "  " << loaded << " = load ptr, ptr " << addr << "\n";
        out << "  call void @_dorothy_release(ptr " << loaded << ")\n";
    }
}

void LLVMGenCtx::pop_scope() {
    if (!scopes.empty()) {
        emit_release_scope(scopes.back());
        scopes.pop_back();
    }
}

void LLVMGenCtx::emit_release_all_scopes() {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        emit_release_scope(*it);
    }
}

// ===== Class resolution helpers =====

static string resolve_expr_class_name(LLVMGenCtx& ctx, Expression* expr) {
    if (!expr) return "";
    if (dynamic_cast<ThisExpr*>(expr)) {
        return ctx.this_class;
    }
    auto* ma = dynamic_cast<MemberAccess*>(expr);
    if (ma) {
        string parent_class = resolve_expr_class_name(ctx, ma->getObject());
        if (!parent_class.empty() && g_class_defs.count(parent_class)) {
            int idx = g_class_defs[parent_class].fieldIndex(ma->getMember());
            if (idx >= 0) {
                const auto& field = g_class_defs[parent_class].fields[idx];
                if (field.type == VarType::CLASS) {
                    return field.struct_name;
                }
            }
        }
        return "";
    }
    auto* var_expr = dynamic_cast<Variable*>(expr);
    if (var_expr) {
        const string& varname = var_expr->getVarName();
        if (varname == "$this" || varname == "this") return ctx.this_class;
        auto it = ctx.class_var_types.find(varname);
        if (it != ctx.class_var_types.end()) return it->second;
    }
    auto* cme = dynamic_cast<CallMethodExp*>(expr);
    if (cme) {
        string parent_class = resolve_expr_class_name(ctx, cme->getObject());
        if (!parent_class.empty() && g_class_defs.count(parent_class)) {
            auto* minfo = g_class_defs[parent_class].getMethod(cme->getMethodName());
            if (minfo && minfo->ret_type == VarType::CLASS) {
                return minfo->ret_type_name;
            }
        }
    }
    auto* ci = dynamic_cast<ClassInit*>(expr);
    if (ci) {
        return ci->getClassName();
    }
    return "";
}

static bool is_class_returning_call(LLVMGenCtx& ctx, Expression* expr) {
    if (!expr) return false;
    if (dynamic_cast<ClassInit*>(expr)) return true;
    if (auto* cfe = dynamic_cast<CallFuncExp*>(expr)) {
        if (ctx.func_return_class.count(cfe->getId())) return true;
    }
    if (auto* cme = dynamic_cast<CallMethodExp*>(expr)) {
        string cname = resolve_expr_class_name(ctx, cme->getObject());
        if (!cname.empty() && g_class_defs.count(cname)) {
            auto* minfo = g_class_defs[cname].getMethod(cme->getMethodName());
            if (minfo && minfo->ret_type == VarType::CLASS) return true;
        }
    }
    return false;
}

// ===== Class emission helpers =====

static void emit_class_destructor(LLVMGenCtx& ctx, const string& cname, const ClassDefInfo& cdef) {
    ctx.out << "define void @" << cname << ".destructor(ptr %this) {\nentry:\n";
    for (int fi = 0; fi < (int)cdef.fields.size(); fi++) {
        if (cdef.fields[fi].type == VarType::CLASS) {
            string fptr = ctx.fresh("dtor.fptr");
            ctx.out << "  " << fptr << " = getelementptr %class." << cname
                    << ", ptr %this, i32 0, i32 " << (fi + 2) << "\n";
            string child = ctx.fresh("dtor.child");
            ctx.out << "  " << child << " = load ptr, ptr " << fptr << "\n";
            ctx.out << "  call void @_dorothy_release(ptr " << child << ")\n";
        }
    }
    ctx.out << "  call void @free(ptr %this)\n";
    ctx.out << "  ret void\n";
    ctx.out << "}\n\n";
}

static void emit_class_constructor(LLVMGenCtx& ctx, const string& cname, const ClassDefInfo& cdef) {
    if (!cdef.constructor) return;
    ctx.vars.clear();
    ctx.var_types.clear();
    ctx.const_vars.clear();
    ctx.struct_field_ptrs.clear();
    ctx.struct_var_types.clear();
    ctx.struct_subfield_types.clear();
    ctx.class_var_types.clear();
    ctx.this_var.clear();
    ctx.this_ptr_reg = "%this";
    ctx.this_class = cname;
    ctx.sret_field_ptrs.clear();
    ctx.current_ret_struct.clear();
    ctx.current_ret_is_class = false;
    ctx.terminated = false;
    ctx.current_function = cname + ".constructor";
    ctx.scopes.clear();
    ctx.push_scope();

    vector<pair<string, string>> fn_params;
    fn_params.push_back({"ptr", "%this"});
    int flat_idx = 0;
    for (auto& p : cdef.constructor->params) {
        fn_params.push_back({llvm_type_str(p.second), "%param." + to_string(flat_idx++)});
    }

    ctx.out << "define void @" << cname << ".constructor(";
    for (int i = 0; i < (int)fn_params.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << fn_params[i].first << " " << fn_params[i].second;
    }
    ctx.out << ") {\nentry:\n";

    // Allocate and store parameters
    flat_idx = 0;
    for (auto& p : cdef.constructor->params) {
        int n = ctx.counter++;
        string tstr = llvm_type_str(p.second);
        string ptr = "%" + p.first + ".addr." + to_string(n);
        string param_reg = "%param." + to_string(flat_idx++);
        ctx.out << "  " << ptr << " = alloca " << tstr << "\n";
        ctx.out << "  store " << tstr << " " << param_reg << ", ptr " << ptr << "\n";
        ctx.vars[p.first] = ptr;
        ctx.var_types[p.first] = p.second;
        if (p.second == VarType::CLASS) {
            ctx.out << "  call void @_dorothy_retain(ptr " << param_reg << ")\n";
            ctx.register_class_var(ptr);
        }
    }

    cdef.constructor->body->llvm_emit(ctx);

    if (!ctx.terminated) {
        ctx.pop_scope();
        ctx.out << "  ret void\n";
    }
    ctx.out << "}\n\n";
}

static void emit_class_method(LLVMGenCtx& ctx, const string& cname, MethodInfo* m) {
    if (m->is_abstract || !m->body) return;
    ctx.vars.clear();
    ctx.var_types.clear();
    ctx.const_vars.clear();
    ctx.struct_field_ptrs.clear();
    ctx.struct_var_types.clear();
    ctx.struct_subfield_types.clear();
    ctx.class_var_types.clear();
    ctx.this_var.clear();
    ctx.this_ptr_reg = "%this";
    ctx.this_class = cname;
    ctx.sret_field_ptrs.clear();
    ctx.current_ret_struct = (m->ret_type == VarType::STRUCT) ? m->ret_type_name : "";
    ctx.current_ret_is_class = (m->ret_type == VarType::CLASS);
    ctx.terminated = false;
    ctx.current_function = cname + "." + m->name;
    ctx.scopes.clear();
    ctx.push_scope();

    string ret_llvm = (m->ret_type == VarType::STRUCT) ? "void" : "i64";

    vector<pair<string, string>> fn_params;
    fn_params.push_back({"ptr", "%this"});
    int flat_idx = 0;
    for (auto* p : m->params) {
        fn_params.push_back({llvm_type_str(p->getType()), "%param." + to_string(flat_idx++)});
    }

    ctx.out << "define " << ret_llvm << " @" << cname << "." << m->name << "(";
    for (int i = 0; i < (int)fn_params.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << fn_params[i].first << " " << fn_params[i].second;
    }
    ctx.out << ") {\nentry:\n";

    // Allocate and store parameters
    flat_idx = 0;
    for (auto* p : m->params) {
        p->llvm_param(ctx, "%param." + to_string(flat_idx++));
    }

    m->body->llvm_emit(ctx);

    if (!ctx.terminated) {
        ctx.pop_scope();
        if (m->ret_type == VarType::STRUCT) {
            ctx.out << "  ret void\n";
        } else {
            ctx.out << "  ret " << ret_llvm << " 0\n";
        }
    }
    ctx.out << "}\n\n";
}

static void emit_all_classes(LLVMGenCtx& ctx) {
    if (ctx.classes_emitted) return;
    ctx.classes_emitted = true;

    emit_string_runtime(ctx);

    for (const auto& pair : g_class_defs) {
        const auto& cdef = pair.second;
        if (cdef.constructor && cdef.constructor->body) {
            cdef.constructor->body->collect_strings(ctx);
        }
        for (auto* m : cdef.vtable_methods) {
            if (m && m->body) m->body->collect_strings(ctx);
        }
        for (const auto& mp : cdef.methods) {
            if (mp.second && mp.second->body) mp.second->body->collect_strings(ctx);
        }
    }

    if (!g_class_defs.empty()) {
        // Declare malloc, free, puts, exit if not already declared by C header
        if (!ctx.c_imported_funcs.count("malloc")) {
            ctx.out << "declare ptr @malloc(i64)\n\n";
            ctx.c_imported_funcs["malloc"] = {"ptr", {"i64"}, false};
        }
        if (!ctx.c_imported_funcs.count("free")) {
            ctx.out << "declare void @free(ptr)\n\n";
            ctx.c_imported_funcs["free"] = {"void", {"ptr"}, false};
        }
        if (!ctx.c_imported_funcs.count("puts")) {
            ctx.out << "declare i32 @puts(ptr)\n\n";
            ctx.c_imported_funcs["puts"] = {"i32", {"ptr"}, false};
        }
        if (!ctx.c_imported_funcs.count("exit")) {
            ctx.out << "declare void @exit(i32)\n\n";
            ctx.c_imported_funcs["exit"] = {"void", {"i32"}, false};
        }

        ctx.out << "@.str.npe = private unnamed_addr constant [57 x i8] c\"NullPointerException: Attempted to access null reference\\00\", align 1\n\n";

        ctx.out << "define void @_dorothy_check_null(ptr %obj) {\n"
                << "entry:\n"
                << "  %is_null = icmp eq ptr %obj, null\n"
                << "  br i1 %is_null, label %npe, label %ok\n"
                << "npe:\n"
                << "  %msg = getelementptr [57 x i8], ptr @.str.npe, i32 0, i32 0\n"
                << "  call i32 @puts(ptr %msg)\n"
                << "  call void @exit(i32 1)\n"
                << "  unreachable\n"
                << "ok:\n"
                << "  ret void\n"
                << "}\n\n";

        // Emit ARC helper functions
        ctx.out << "define void @_dorothy_retain(ptr %obj) {\n"
                << "entry:\n"
                << "  %is_null = icmp eq ptr %obj, null\n"
                << "  br i1 %is_null, label %ret, label %retain.body\n"
                << "retain.body:\n"
                << "  %ref_slot = getelementptr { ptr, i64 }, ptr %obj, i32 0, i32 1\n"
                << "  %cnt = load i64, ptr %ref_slot\n"
                << "  %next = add i64 %cnt, 1\n"
                << "  store i64 %next, ptr %ref_slot\n"
                << "  br label %ret\n"
                << "ret:\n"
                << "  ret void\n"
                << "}\n\n";

        ctx.out << "define void @_dorothy_release(ptr %obj) {\n"
                << "entry:\n"
                << "  %is_null = icmp eq ptr %obj, null\n"
                << "  br i1 %is_null, label %ret, label %rel.body\n"
                << "rel.body:\n"
                << "  %ref_slot = getelementptr { ptr, i64 }, ptr %obj, i32 0, i32 1\n"
                << "  %cnt = load i64, ptr %ref_slot\n"
                << "  %next = sub i64 %cnt, 1\n"
                << "  store i64 %next, ptr %ref_slot\n"
                << "  %is_zero = icmp eq i64 %next, 0\n"
                << "  br i1 %is_zero, label %do_dtor, label %ret\n"
                << "do_dtor:\n"
                << "  %vtable_slot = getelementptr { ptr, i64 }, ptr %obj, i32 0, i32 0\n"
                << "  %vtable_ptr = load ptr, ptr %vtable_slot\n"
                << "  %dtor_slot = getelementptr ptr, ptr %vtable_ptr, i32 0\n"
                << "  %dtor_fn = load ptr, ptr %dtor_slot\n"
                << "  call void %dtor_fn(ptr %obj)\n"
                << "  br label %ret\n"
                << "ret:\n"
                << "  ret void\n"
                << "}\n\n";

        // Class struct type definitions: %class.ClassName = type { ptr, i64, ...fields }
        for (auto& pair : g_class_defs) {
            const auto& cname = pair.first;
            auto& cdef = pair.second;
            ctx.out << "%class." << cname << " = type { ptr, i64";
            for (auto& f : cdef.fields) {
                ctx.out << ", " << llvm_type_str(f.type);
            }
            ctx.out << " }\n";
        }
        ctx.out << "\n";

        // Class vtables: slot 0 is always destructor, slot 1..N are methods
        for (auto& pair : g_class_defs) {
            const auto& cname = pair.first;
            auto& cdef = pair.second;
            if (cdef.is_abstract) continue;
            int n_methods = (int)cdef.vtable_methods.size();
            ctx.out << "@vtable." << cname << " = global [" << (n_methods + 1) << " x ptr] [";
            ctx.out << "ptr @" << cname << ".destructor";
            for (int i = 0; i < n_methods; i++) {
                ctx.out << ", ";
                auto* m = cdef.vtable_methods[i];
                ctx.out << "ptr @" << m->class_name << "." << m->name;
            }
            ctx.out << "]\n";
        }
        ctx.out << "\n";

        // Destructors, Constructors and methods
        for (auto& pair : g_class_defs) {
            const auto& cname = pair.first;
            auto& cdef = pair.second;
            if (!cdef.is_abstract) {
                emit_class_destructor(ctx, cname, cdef);
            }
            if (cdef.constructor) {
                emit_class_constructor(ctx, cname, cdef);
            }
            for (auto& mpair : cdef.methods) {
                auto* minfo = mpair.second;
                if (minfo->class_name == cname && !minfo->is_abstract) {
                    emit_class_method(ctx, cname, minfo);
                }
            }
        }
    }
}

// ===== Expression base =====

string Expression::llvm_lval(LLVMGenCtx& ctx) {
    throw CompileError("expression cannot be used as left-value");
}

// ===== DeclVar =====

void DeclVar::llvm_emit(LLVMGenCtx& ctx) {
    if (_type == VarType::CLASS) {
        if (!g_class_defs.count(_struct_name))
            throw CompileError(("undefined class type: " + _struct_name).c_str());
        int n = ctx.counter++;
        string ptr = "%" + _id + ".addr." + to_string(n);
        ctx.out << "  " << ptr << " = alloca ptr\n";
        ctx.out << "  store ptr null, ptr " << ptr << "\n";
        ctx.vars[_id] = ptr;
        ctx.var_types[_id] = VarType::CLASS;
        ctx.class_var_types[_id] = _struct_name;
        ctx.class_var_nullable[_id] = _is_nullable;
        if (_is_const) ctx.const_vars.insert(_id);
        ctx.register_class_var(ptr);
        return;
    }
    if (_type == VarType::STRING) {
        int n = ctx.counter++;
        string ptr = "%" + _id + ".addr." + to_string(n);
        ctx.out << "  " << ptr << " = alloca ptr\n";
        ctx.out << "  store ptr null, ptr " << ptr << "\n";
        ctx.vars[_id] = ptr;
        ctx.var_types[_id] = VarType::STRING;
        if (_is_const) ctx.const_vars.insert(_id);
        return;
    }
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
    // ===== Type inference: resolve INFERRED before allocating =====
    if (_type == VarType::INFERRED) {
        auto* ci = dynamic_cast<ClassInit*>(_init);
        if (ci) {
            _type = VarType::CLASS;
            _struct_name = ci->getClassName();
        } else if (dynamic_cast<StringExp*>(_init)) {
            _type = VarType::STRING;
        } else if (auto* cfe = dynamic_cast<CallFuncExp*>(_init)) {
            if (ctx.func_return_struct.count(cfe->getId())) {
                _type = VarType::STRUCT;
                _struct_name = ctx.func_return_struct[cfe->getId()];
            }
        } else if (auto* var_expr = dynamic_cast<Variable*>(_init)) {
            if (ctx.class_var_types.count(var_expr->getVarName())) {
                _type = VarType::CLASS;
                _struct_name = ctx.class_var_types[var_expr->getVarName()];
            } else if (ctx.struct_var_types.count(var_expr->getVarName())) {
                _type = VarType::STRUCT;
                _struct_name = ctx.struct_var_types[var_expr->getVarName()];
            } else {
                _type = _init->llvm_etype(ctx);
            }
        } else {
            _type = _init->llvm_etype(ctx);
        }
    }
    // ===== End type inference =====

    DeclVar::llvm_emit(ctx);  // alloca + zero-init + type/const tracking

    if (_type == VarType::CLASS) {
        string val = _init->llvm_rval(ctx);
        string ptr_val = ctx.fresh("init.ptr");
        ctx.out << "  " << ptr_val << " = inttoptr i64 " << val << " to ptr\n";
        bool rhs_is_new_or_returned = is_class_returning_call(ctx, _init);
        if (!rhs_is_new_or_returned) {
            ctx.out << "  call void @_dorothy_retain(ptr " << ptr_val << ")\n";
        }
        ctx.out << "  store ptr " << ptr_val << ", ptr " << ctx.vars[_id] << "\n";
        return;
    }

    if (_type == VarType::STRING) {
        string val = _init->llvm_rval(ctx);
        string ptr_val = ctx.fresh("init.str");
        ctx.out << "  " << ptr_val << " = inttoptr i64 " << val << " to ptr\n";
        ctx.out << "  store ptr " << ptr_val << ", ptr " << ctx.vars[_id] << "\n";
        return;
    }

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
            for (const auto& param : sdef.constructor->params) {
                const auto& pname = param.first;
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
    if (_type == VarType::CLASS) {
        ctx.class_var_types[_id] = _struct_name;
        ctx.class_var_nullable[_id] = _is_nullable;
        ctx.out << "  call void @_dorothy_retain(ptr " << param_reg << ")\n";
        ctx.register_class_var(ptr);
    }
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
    ctx.array_elem_types[_id] = _type;   // element type (e.g. i32, i8)
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
    ctx.push_scope();
    for (auto st : _statements) {
        if (ctx.terminated) break;
        st->llvm_emit(ctx);
    }
    if (!ctx.terminated) {
        ctx.pop_scope();
    } else {
        if (!ctx.scopes.empty()) ctx.scopes.pop_back();
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
        ctx.emit_release_all_scopes();
        ctx.out << "  ret void\n";
        ctx.terminated = true;
        return;
    }

    string val = _exp->llvm_rval(ctx);
    VarType etype = _exp->llvm_etype(ctx);

    // If returning a CLASS, retain it before releasing local scopes
    if (ctx.current_ret_is_class) {
        string ret_ptr = ctx.fresh("ret.class.ptr");
        ctx.out << "  " << ret_ptr << " = inttoptr i64 " << val << " to ptr\n";
        ctx.out << "  call void @_dorothy_retain(ptr " << ret_ptr << ")\n";
    }

    ctx.emit_release_all_scopes();

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
                if (tgt == VarType::CLASS || tgt == VarType::STRING) {
                    string r = ctx.fresh("arg.ptr");
                    ctx.out << "  " << r << " = inttoptr i64 " << val << " to ptr\n";
                    result.push_back({"ptr", r});
                } else {
                    val = llvm_coerce(ctx, val, src_c, tgt);
                    result.push_back({llvm_type_str(tgt), val});
                }
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

    if (ctx.func_return_class.count(_id)) {
        string ret_ptr = ctx.fresh("unused.class.ptr");
        ctx.out << "  " << ret_ptr << " = inttoptr i64 " << reg << " to ptr\n";
        ctx.out << "  call void @_dorothy_release(ptr " << ret_ptr << ")\n";
    }
}

// ===== ExpressionSt =====

void ExpressionSt::llvm_emit(LLVMGenCtx& ctx) {
    string val = _exp->llvm_rval(ctx);
    if (is_class_returning_call(ctx, _exp)) {
        string ret_ptr = ctx.fresh("unused.class.ptr");
        ctx.out << "  " << ret_ptr << " = inttoptr i64 " << val << " to ptr\n";
        ctx.out << "  call void @_dorothy_release(ptr " << ret_ptr << ")\n";
    }
}

// ===== Function =====

void Function::llvm_pre_register(LLVMGenCtx& ctx) {
    if (_ret_type == VarType::STRUCT && !_ret_struct_name.empty())
        ctx.func_return_struct[_id] = _ret_struct_name;
    if (_ret_type == VarType::CLASS)
        ctx.func_return_class.insert(_id);
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
    emit_all_classes(ctx);
    if (_body) _body->collect_strings(ctx);

    ctx.vars.clear();
    ctx.var_types.clear();
    ctx.const_vars.clear();
    ctx.struct_field_ptrs.clear();
    ctx.struct_var_types.clear();
    ctx.struct_subfield_types.clear();
    ctx.class_var_types.clear();
    ctx.this_var.clear();
    ctx.this_ptr_reg.clear();
    ctx.this_class.clear();
    ctx.sret_field_ptrs.clear();
    ctx.current_ret_struct = (_ret_type == VarType::STRUCT) ? _ret_struct_name : "";
    ctx.current_ret_is_class = (_ret_type == VarType::CLASS);
    ctx.terminated = false;
    ctx.current_function = _id;
    ctx.scopes.clear();
    ctx.push_scope();

    bool is_main = (_id == "main");
    bool returns_struct = (_ret_type == VarType::STRUCT && !_ret_struct_name.empty());

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
        ctx.pop_scope();
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
    for (auto& pair : funcs) {
        const auto& name = pair.first;
        auto& info = pair.second;
        if (ctx.c_imported_funcs.count(name)) continue;
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
    VarType tgt_declared = _leftside->llvm_declared_type(ctx);
    if (tgt_declared == VarType::CLASS) {
        if (dynamic_cast<NullExp*>(_expr)) {
            if (!varname.empty() && ctx.class_var_nullable.count(varname) && !ctx.class_var_nullable[varname]) {
                throw CompileError(("cannot assign null to non-nullable variable: " + varname).c_str());
            }
        }
        string ptr_val = ctx.fresh("assign.ptr");
        ctx.out << "  " << ptr_val << " = inttoptr i64 " << val << " to ptr\n";
        bool rhs_is_new_or_returned = is_class_returning_call(ctx, _expr);
        if (!rhs_is_new_or_returned) {
            ctx.out << "  call void @_dorothy_retain(ptr " << ptr_val << ")\n";
        }
        string old_val = ctx.fresh("old.ptr");
        ctx.out << "  " << old_val << " = load ptr, ptr " << ptr << "\n";
        ctx.out << "  store ptr " << ptr_val << ", ptr " << ptr << "\n";
        ctx.out << "  call void @_dorothy_release(ptr " << old_val << ")\n";
        return val;
    }
    if (tgt_declared == VarType::STRING) {
        string ptr_val = ctx.fresh("assign.str");
        ctx.out << "  " << ptr_val << " = inttoptr i64 " << val << " to ptr\n";
        ctx.out << "  store ptr " << ptr_val << ", ptr " << ptr << "\n";
        return val;
    }
    VarType src_canonical = _expr->llvm_etype(ctx);
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
        ctx.out << "  " << reg << " = fop " << "double " << l << ", " << r << "\n";
        return reg;
    }
    string reg = ctx.fresh();
    ctx.out << "  " << reg << " = " << iop << " i64 " << l << ", " << r << "\n";
    return reg;
}

string AddExp::llvm_rval(LLVMGenCtx& ctx) {
    VarType lt = _left->llvm_etype(ctx);
    VarType rt = _right->llvm_etype(ctx);
    if (lt == VarType::STRING || rt == VarType::STRING) {
        string l = _left->llvm_rval(ctx);
        string r = _right->llvm_rval(ctx);
        string l_ptr = ctx.fresh("str.l");
        string r_ptr = ctx.fresh("str.r");
        ctx.out << "  " << l_ptr << " = inttoptr i64 " << l << " to ptr\n";
        ctx.out << "  " << r_ptr << " = inttoptr i64 " << r << " to ptr\n";
        string res_ptr = ctx.fresh("str.concat");
        ctx.out << "  " << res_ptr << " = call ptr @_dorothy_str_concat(ptr " << l_ptr << ", ptr " << r_ptr << ")\n";
        string res_i64 = ctx.fresh("str.concat.i64");
        ctx.out << "  " << res_i64 << " = ptrtoint ptr " << res_ptr << " to i64\n";
        return res_i64;
    }
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
    if (et == VarType::STRING) {
        string l_ptr = ctx.fresh("str.cmp.l");
        string r_ptr = ctx.fresh("str.cmp.r");
        ctx.out << "  " << l_ptr << " = inttoptr i64 " << l << " to ptr\n";
        ctx.out << "  " << r_ptr << " = inttoptr i64 " << r << " to ptr\n";
        string diff = ctx.fresh("str.diff");
        ctx.out << "  " << diff << " = call i32 @_dorothy_str_cmp(ptr " << l_ptr << ", ptr " << r_ptr << ")\n";
        ctx.out << "  " << cmp << " = icmp " << ipred << " i32 " << diff << ", 0\n";
    } else if (et == VarType::DOUBLE) {
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
    string cname = resolve_expr_class_name(ctx, _object);
    if (!cname.empty() && g_class_defs.count(cname)) {
        string obj_ptr;
        auto* this_expr = dynamic_cast<ThisExpr*>(_object);
        auto* var_expr = dynamic_cast<Variable*>(_object);
        if (this_expr || (var_expr && (var_expr->getVarName() == "$this" || var_expr->getVarName() == "this"))) {
            obj_ptr = ctx.this_ptr_reg;
        } else if (var_expr && ctx.class_var_types.count(var_expr->getVarName())) {
            const string& varname = var_expr->getVarName();
            string loaded = ctx.fresh("obj.ptr");
            ctx.out << "  " << loaded << " = load ptr, ptr " << ctx.vars[varname] << "\n";
            obj_ptr = loaded;
        } else {
            string obj_i64 = _object->llvm_rval(ctx);
            obj_ptr = ctx.fresh("obj.ptr");
            ctx.out << "  " << obj_ptr << " = inttoptr i64 " << obj_i64 << " to ptr\n";
        }
        ctx.out << "  call void @_dorothy_check_null(ptr " << obj_ptr << ")\n";

        const auto& cdef = g_class_defs[cname];
        int fidx = cdef.fieldIndex(_member);
        if (fidx < 0) throw CompileError(("no field '" + _member + "' in class " + cname).c_str());
        VarType ftype = cdef.fields[fidx].type;
        string fptr = ctx.fresh("field.ptr");
        ctx.out << "  " << fptr << " = getelementptr %class." << cname
                << ", ptr " << obj_ptr << ", i32 0, i32 " << (fidx + 2) << "\n";
        if (ftype == VarType::CLASS || ftype == VarType::STRING) {
            string reg = ctx.fresh("field.ptr");
            ctx.out << "  " << reg << " = load ptr, ptr " << fptr << "\n";
            string r_i64 = ctx.fresh("field.i64");
            ctx.out << "  " << r_i64 << " = ptrtoint ptr " << reg << " to i64\n";
            return r_i64;
        }
        string tstr = llvm_type_str(ftype);
        string reg = ctx.fresh(cname + "_" + _member);
        ctx.out << "  " << reg << " = load " << tstr << ", ptr " << fptr << "\n";
        return llvm_to_canonical(ctx, reg, ftype);
    }

    // Struct field
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
    string cname = resolve_expr_class_name(ctx, _object);
    if (!cname.empty() && g_class_defs.count(cname)) {
        string obj_ptr;
        auto* this_expr = dynamic_cast<ThisExpr*>(_object);
        auto* var_expr = dynamic_cast<Variable*>(_object);
        if (this_expr || (var_expr && (var_expr->getVarName() == "$this" || var_expr->getVarName() == "this"))) {
            obj_ptr = ctx.this_ptr_reg;
        } else if (var_expr && ctx.class_var_types.count(var_expr->getVarName())) {
            const string& varname = var_expr->getVarName();
            string loaded = ctx.fresh("obj.ptr");
            ctx.out << "  " << loaded << " = load ptr, ptr " << ctx.vars[varname] << "\n";
            obj_ptr = loaded;
        } else {
            string obj_i64 = _object->llvm_rval(ctx);
            obj_ptr = ctx.fresh("obj.ptr");
            ctx.out << "  " << obj_ptr << " = inttoptr i64 " << obj_i64 << " to ptr\n";
        }
        ctx.out << "  call void @_dorothy_check_null(ptr " << obj_ptr << ")\n";

        const auto& cdef = g_class_defs[cname];
        int fidx = cdef.fieldIndex(_member);
        if (fidx < 0) throw CompileError(("no field '" + _member + "' in class " + cname).c_str());
        string fptr = ctx.fresh("field.ptr");
        ctx.out << "  " << fptr << " = getelementptr %class." << cname
                << ", ptr " << obj_ptr << ", i32 0, i32 " << (fidx + 2) << "\n";
        return fptr;
    }

    // Struct field
    string varname = resolve_struct_var(ctx, _object->getVarName());
    string path = getFieldPath();
    string ptr = ctx.struct_field_ptrs[varname][path];
    if (ptr.empty())
        throw CompileError(("struct field '" + path + "' is a struct type; assign field-by-field").c_str());
    return ptr;
}

VarType MemberAccess::llvm_declared_type(LLVMGenCtx& ctx) const {
    string cname = resolve_expr_class_name(ctx, _object);
    if (!cname.empty() && g_class_defs.count(cname)) {
        int idx = g_class_defs[cname].fieldIndex(_member);
        if (idx >= 0) return g_class_defs[cname].fields[idx].type;
    }
    string varname = resolve_struct_var(ctx, _object->getVarName());
    return member_field_type(ctx, varname, getFieldPath());
}

VarType MemberAccess::llvm_etype(LLVMGenCtx& ctx) const {
    return canonical_type(llvm_declared_type(ctx));
}

// ===== ThisExpr =====

string ThisExpr::llvm_rval(LLVMGenCtx& ctx) {
    if (!ctx.this_ptr_reg.empty()) {
        string r = ctx.fresh("this.i64");
        ctx.out << "  " << r << " = ptrtoint ptr " << ctx.this_ptr_reg << " to i64\n";
        return r;
    }
    throw CompileError("'this' cannot be used as a standalone rvalue in this context");
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

VarType Variable::llvm_declared_type(LLVMGenCtx& ctx) const {
    if (ctx.class_var_types.count(_id)) return VarType::CLASS;
    auto it = ctx.var_types.find(_id);
    return (it != ctx.var_types.end()) ? it->second : VarType::LONG;
}

string Variable::llvm_rval(LLVMGenCtx& ctx) {
    auto it = ctx.vars.find(_id);
    if (it == ctx.vars.end()) {
        if (ctx.struct_var_types.count(_id))
            throw CompileError(("struct variable cannot be used as a value: " + _id).c_str());
        if (_id == "$this" || _id == "this") {
            if (!ctx.this_ptr_reg.empty()) {
                string r = ctx.fresh("this.i64");
                ctx.out << "  " << r << " = ptrtoint ptr " << ctx.this_ptr_reg << " to i64\n";
                return r;
            }
        }
        throw CompileError(("undefined variable: " + _id).c_str());
    }
    VarType declared = llvm_declared_type(ctx);
    if (declared == VarType::CLASS || declared == VarType::STRING) {
        string reg = ctx.fresh(_id + ".ptr");
        ctx.out << "  " << reg << " = load ptr, ptr " << it->second << "\n";
        string int_reg = ctx.fresh(_id + ".i64");
        ctx.out << "  " << int_reg << " = ptrtoint ptr " << reg << " to i64\n";
        return int_reg;
    }
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

VarType ArrayIndex::llvm_declared_type(LLVMGenCtx& ctx) const {
    const string& varname = _pointer->getVarName();
    auto it = ctx.array_elem_types.find(varname);
    if (it != ctx.array_elem_types.end()) {
        return it->second;
    }
    return VarType::CHAR;
}

VarType ArrayIndex::llvm_etype(LLVMGenCtx& ctx) const {
    return canonical_type(llvm_declared_type(ctx));
}

string ArrayIndex::llvm_rval(LLVMGenCtx& ctx) {
    VarType elem_type = llvm_declared_type(ctx);
    string tstr = llvm_type_str(elem_type);
    const string& varname = _pointer->getVarName();
    string arr_ptr;
    auto it = ctx.array_data_ptrs.find(varname);
    if (it != ctx.array_data_ptrs.end()) {
        arr_ptr = it->second;
    } else {
        string base = _pointer->llvm_rval(ctx);
        arr_ptr = ctx.fresh("arr.ptr");
        ctx.out << "  " << arr_ptr << " = inttoptr i64 " << base << " to ptr\n";
    }
    string idx = _index->llvm_rval(ctx);
    string elem_ptr = ctx.fresh("elem.ptr");
    ctx.out << "  " << elem_ptr << " = getelementptr " << tstr << ", ptr " << arr_ptr
            << ", i64 " << idx << "\n";
    string loaded = ctx.fresh();
    ctx.out << "  " << loaded << " = load " << tstr << ", ptr " << elem_ptr << "\n";
    if (is_float_type(elem_type)) {
        if (elem_type == VarType::FLOAT) {
            return llvm_promote_to_double(ctx, loaded, VarType::FLOAT);
        }
        return loaded;
    }
    if (elem_type == VarType::CHAR) {
        string reg = ctx.fresh("zext");
        ctx.out << "  " << reg << " = zext i8 " << loaded << " to i64\n";
        return reg;
    }
    if (elem_type != VarType::LONG) {
        string reg = ctx.fresh("sext");
        ctx.out << "  " << reg << " = sext " << tstr << " " << loaded << " to i64\n";
        return reg;
    }
    return loaded;
}

string ArrayIndex::llvm_lval(LLVMGenCtx& ctx) {
    VarType elem_type = llvm_declared_type(ctx);
    string tstr = llvm_type_str(elem_type);
    const string& varname = _pointer->getVarName();
    string arr_ptr;
    auto it = ctx.array_data_ptrs.find(varname);
    if (it != ctx.array_data_ptrs.end()) {
        arr_ptr = it->second;
    } else {
        string base = _pointer->llvm_rval(ctx);
        arr_ptr = ctx.fresh("arr.ptr");
        ctx.out << "  " << arr_ptr << " = inttoptr i64 " << base << " to ptr\n";
    }
    string idx = _index->llvm_rval(ctx);
    string elem_ptr = ctx.fresh("elem.ptr");
    ctx.out << "  " << elem_ptr << " = getelementptr " << tstr << ", ptr " << arr_ptr
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

// ===== ClassInit =====

string ClassInit::llvm_rval(LLVMGenCtx& ctx) {
    if (!g_class_defs.count(_class_name))
        throw CompileError(("undefined class: " + _class_name).c_str());
    const auto& cdef = g_class_defs[_class_name];
    if (cdef.is_abstract)
        throw CompileError(("cannot instantiate abstract class: " + _class_name).c_str());

    // Allocate memory via malloc
    string size_ptr = ctx.fresh("size.ptr");
    ctx.out << "  " << size_ptr << " = getelementptr %class." << _class_name << ", ptr null, i32 1\n";
    string size_i64 = ctx.fresh("size.i64");
    ctx.out << "  " << size_i64 << " = ptrtoint ptr " << size_ptr << " to i64\n";
    string raw_mem = ctx.fresh("raw.inst");
    ctx.out << "  " << raw_mem << " = call ptr @malloc(i64 " << size_i64 << ")\n";

    // Store vtable pointer into slot 0
    string vtable_gep = ctx.fresh("vtable.slot");
    ctx.out << "  " << vtable_gep << " = getelementptr %class." << _class_name
            << ", ptr " << raw_mem << ", i32 0, i32 0\n";
    ctx.out << "  store ptr @vtable." << _class_name << ", ptr " << vtable_gep << "\n";

    // Store initial reference count = 1 into slot 1
    string ref_slot = ctx.fresh("ref.slot");
    ctx.out << "  " << ref_slot << " = getelementptr %class." << _class_name
            << ", ptr " << raw_mem << ", i32 0, i32 1\n";
    ctx.out << "  store i64 1, ptr " << ref_slot << "\n";

    // Initialize fields to 0 (starting at slot 2)
    for (int fi = 0; fi < (int)cdef.fields.size(); fi++) {
        string fptr = ctx.fresh("init.fptr");
        string tstr = llvm_type_str(cdef.fields[fi].type);
        ctx.out << "  " << fptr << " = getelementptr %class." << _class_name
                << ", ptr " << raw_mem << ", i32 0, i32 " << (fi + 2) << "\n";
        if (is_float_type(cdef.fields[fi].type)) {
            ctx.out << "  store " << tstr << " 0.0, ptr " << fptr << "\n";
        } else if (cdef.fields[fi].type == VarType::CLASS || cdef.fields[fi].type == VarType::STRING) {
            ctx.out << "  store ptr null, ptr " << fptr << "\n";
        } else {
            ctx.out << "  store " << tstr << " 0, ptr " << fptr << "\n";
        }
    }

    // Call constructor if exists
    if (cdef.constructor) {
        vector<pair<string, string>> ctor_args;
        ctor_args.push_back({"ptr", raw_mem});
        for (int i = 0; i < (int)_args.size(); i++) {
            string aval = _args[i]->llvm_rval(ctx);
            VarType ac = _args[i]->llvm_etype(ctx);
            if (i < (int)cdef.constructor->params.size()) {
                VarType pt = cdef.constructor->params[i].second;
                if (pt == VarType::CLASS || pt == VarType::STRING) {
                    string r = ctx.fresh("ctor.ptr");
                    ctx.out << "  " << r << " = inttoptr i64 " << aval << " to ptr\n";
                    ctor_args.push_back({"ptr", r});
                } else {
                    aval = llvm_coerce(ctx, aval, ac, pt);
                    ctor_args.push_back({llvm_type_str(pt), aval});
                }
            } else {
                ctor_args.push_back({"i64", aval});
            }
        }
        ctx.out << "  call void @" << _class_name << ".constructor(";
        for (int i = 0; i < (int)ctor_args.size(); i++) {
            if (i > 0) ctx.out << ", ";
            ctx.out << ctor_args[i].first << " " << ctor_args[i].second;
        }
        ctx.out << ")\n";
    }

    // Return instance pointer as i64 (canonical)
    string ret_i64 = ctx.fresh("inst.i64");
    ctx.out << "  " << ret_i64 << " = ptrtoint ptr " << raw_mem << " to i64\n";
    return ret_i64;
}

// ===== CallMethodExp =====

string CallMethodExp::llvm_rval(LLVMGenCtx& ctx) {
    string obj_ptr;
    string cname = resolve_expr_class_name(ctx, _object);

    auto* this_expr = dynamic_cast<ThisExpr*>(_object);
    auto* var_expr = dynamic_cast<Variable*>(_object);
    if (this_expr || (var_expr && (var_expr->getVarName() == "$this" || var_expr->getVarName() == "this"))) {
        obj_ptr = ctx.this_ptr_reg;
    } else if (var_expr && ctx.class_var_types.count(var_expr->getVarName())) {
        const string& varname = var_expr->getVarName();
        string loaded_ptr = ctx.fresh("obj.ptr");
        ctx.out << "  " << loaded_ptr << " = load ptr, ptr " << ctx.vars[varname] << "\n";
        obj_ptr = loaded_ptr;
    } else {
        string obj_i64 = _object->llvm_rval(ctx);
        obj_ptr = ctx.fresh("obj.ptr");
        ctx.out << "  " << obj_ptr << " = inttoptr i64 " << obj_i64 << " to ptr\n";
    }
    ctx.out << "  call void @_dorothy_check_null(ptr " << obj_ptr << ")\n";

    if (cname.empty() || !g_class_defs.count(cname)) {
        throw CompileError(("cannot resolve class for method call: " + _method_name).c_str());
    }

    const auto& cdef = g_class_defs[cname];
    int vtable_idx = cdef.getMethodVtableIndex(_method_name);
    if (vtable_idx < 0) {
        throw CompileError(("no method '" + _method_name + "' in class " + cname).c_str());
    }
    auto* minfo = cdef.getMethod(_method_name);

    // Load vtable pointer from %obj_ptr (index 0)
    string vtable_addr = ctx.fresh("vtable.addr");
    ctx.out << "  " << vtable_addr << " = getelementptr ptr, ptr " << obj_ptr << ", i32 0\n";
    string vtable_ptr = ctx.fresh("vtable.ptr");
    ctx.out << "  " << vtable_ptr << " = load ptr, ptr " << vtable_addr << "\n";

    // Load function pointer from vtable at vtable_idx + 1 (slot 0 is destructor)
    string fn_addr = ctx.fresh("fn.addr");
    ctx.out << "  " << fn_addr << " = getelementptr ptr, ptr " << vtable_ptr << ", i32 " << (vtable_idx + 1) << "\n";
    string fn_ptr = ctx.fresh("fn.ptr");
    ctx.out << "  " << fn_ptr << " = load ptr, ptr " << fn_addr << "\n";

    // Build signature and call
    string ret_str = (minfo->ret_type == VarType::STRUCT) ? "void" : "i64";
    string fn_sig = ret_str + " (ptr";
    for (auto* p : minfo->params) {
        fn_sig += ", " + llvm_type_str(p->getType());
    }
    fn_sig += ")";

    vector<pair<string, string>> call_args;
    call_args.push_back({"ptr", obj_ptr});
    for (int i = 0; i < (int)_args.size(); i++) {
        string aval = _args[i]->llvm_rval(ctx);
        VarType ac = _args[i]->llvm_etype(ctx);
        if (i < (int)minfo->params.size()) {
            VarType pt = minfo->params[i]->getType();
            if (pt == VarType::CLASS || pt == VarType::STRING) {
                string r = ctx.fresh("arg.ptr");
                ctx.out << "  " << r << " = inttoptr i64 " << aval << " to ptr\n";
                call_args.push_back({"ptr", r});
            } else {
                aval = llvm_coerce(ctx, aval, ac, pt);
                call_args.push_back({llvm_type_str(pt), aval});
            }
        } else {
            call_args.push_back({"i64", aval});
        }
    }

    string call_res = (ret_str != "void") ? ctx.fresh("call.res") : "";
    ctx.out << "  ";
    if (ret_str != "void") ctx.out << call_res << " = ";
    ctx.out << "call " << fn_sig << " " << fn_ptr << "(";
    for (int i = 0; i < (int)call_args.size(); i++) {
        if (i > 0) ctx.out << ", ";
        ctx.out << call_args[i].first << " " << call_args[i].second;
    }
    ctx.out << ")\n";

    if (ret_str == "void") return "0";
    return call_res;
}

// ===== CallMethodSt =====

void CallMethodSt::llvm_emit(LLVMGenCtx& ctx) {
    _call->llvm_rval(ctx);
}

// ===== ClassDef =====

void ClassDef::llvm_emit(LLVMGenCtx& ctx) {
    emit_all_classes(ctx);
}

// ===== NullExp =====

string NullExp::llvm_rval(LLVMGenCtx&) {
    return "0";
}

// ===== StringExp =====

void StringExp::collect_strings(LLVMGenCtx& ctx) {
    emit_string_runtime(ctx);
    if (!ctx.str_literal_map.count(_str_val)) {
        string name = "@.str." + to_string(ctx.str_lit_counter++);
        ctx.str_literal_map[_str_val] = name;
        int len = (int)_str_val.length() + 1;
        string esc = llvm_escape_string(_str_val);
        ctx.out << name << " = private unnamed_addr constant [" << len << " x i8] c\"" << esc << "\\00\", align 1\n\n";
    }
}

string StringExp::llvm_rval(LLVMGenCtx& ctx) {
    collect_strings(ctx);
    string global_name = ctx.str_literal_map[_str_val];
    int len = (int)_str_val.length() + 1;
    string reg = ctx.fresh("str.ptr");
    ctx.out << "  " << reg << " = getelementptr [" << len << " x i8], ptr " << global_name << ", i64 0, i64 0\n";
    string r_i64 = ctx.fresh("str.i64");
    ctx.out << "  " << r_i64 << " = ptrtoint ptr " << reg << " to i64\n";
    return r_i64;
}

// ===== collect_strings implementations =====

void InitializedDeclVar::collect_strings(LLVMGenCtx& ctx) {
    if (_init) _init->collect_strings(ctx);
}

void InitializedDeclArrayVar::collect_strings(LLVMGenCtx& ctx) {
    for (auto* v : _values) if (v) v->collect_strings(ctx);
}

void DeclVarSt::collect_strings(LLVMGenCtx& ctx) {
    if (_decl) _decl->collect_strings(ctx);
}

void IfSt::collect_strings(LLVMGenCtx& ctx) {
    if (_cond) _cond->collect_strings(ctx);
    if (_truest) _truest->collect_strings(ctx);
    if (_falsest) _falsest->collect_strings(ctx);
}

void WhileSt::collect_strings(LLVMGenCtx& ctx) {
    if (_cond) _cond->collect_strings(ctx);
    if (_body) _body->collect_strings(ctx);
}

void ForSt::collect_strings(LLVMGenCtx& ctx) {
    if (_init) _init->collect_strings(ctx);
    if (_cond) _cond->collect_strings(ctx);
    if (_proceed) _proceed->collect_strings(ctx);
    if (_body) _body->collect_strings(ctx);
}

void CallFuncSt::collect_strings(LLVMGenCtx& ctx) {
    for (auto* a : _args) if (a) a->collect_strings(ctx);
}

void ReturnSt::collect_strings(LLVMGenCtx& ctx) {
    if (_exp) _exp->collect_strings(ctx);
}

void Block::collect_strings(LLVMGenCtx& ctx) {
    for (auto* s : _statements) if (s) s->collect_strings(ctx);
}

void ExpressionSt::collect_strings(LLVMGenCtx& ctx) {
    if (_exp) _exp->collect_strings(ctx);
}

void Assign::collect_strings(LLVMGenCtx& ctx) {
    if (_leftside) _leftside->collect_strings(ctx);
    if (_expr) _expr->collect_strings(ctx);
}

void AddExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void SubExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void MulExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void DivExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void ModExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void EQExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void NEExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void LTExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void LEExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void GTExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void GEExp::collect_strings(LLVMGenCtx& ctx) {
    if (_left) _left->collect_strings(ctx);
    if (_right) _right->collect_strings(ctx);
}

void ArrayIndex::collect_strings(LLVMGenCtx& ctx) {
    if (_pointer) _pointer->collect_strings(ctx);
    if (_index) _index->collect_strings(ctx);
}

void Address::collect_strings(LLVMGenCtx& ctx) {
    if (_exp) _exp->collect_strings(ctx);
}

void Access::collect_strings(LLVMGenCtx& ctx) {
    if (_rightside) _rightside->collect_strings(ctx);
}

void CallFuncExp::collect_strings(LLVMGenCtx& ctx) {
    for (auto* a : _args) if (a) a->collect_strings(ctx);
}

void StructInit::collect_strings(LLVMGenCtx& ctx) {
    for (auto* a : _args) if (a) a->collect_strings(ctx);
}

void MemberAccess::collect_strings(LLVMGenCtx& ctx) {
    if (_object) _object->collect_strings(ctx);
}

void ClassInit::collect_strings(LLVMGenCtx& ctx) {
    for (auto* a : _args) if (a) a->collect_strings(ctx);
}

void CallMethodExp::collect_strings(LLVMGenCtx& ctx) {
    if (_object) _object->collect_strings(ctx);
    for (auto* a : _args) if (a) a->collect_strings(ctx);
}

void CallMethodSt::collect_strings(LLVMGenCtx& ctx) {
    if (_call) _call->collect_strings(ctx);
}


