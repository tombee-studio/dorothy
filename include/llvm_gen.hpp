/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "./vartype.hpp"

struct ParamInfo {
    VarType type;
    std::string struct_name;  // non-empty when type == VarType::STRUCT
};

// Signature info for a C function imported via a C header file.
struct CImportedFunc {
    std::string ret_type;            // LLVM return type: "i32", "i64", "ptr", "void", etc.
    std::vector<std::string> param_types;  // LLVM types for known (non-variadic) params
    bool is_variadic;
};

struct LLVMGenCtx {
    std::ostream& out;
    int counter;
    std::map<std::string, std::string> vars;
    std::map<std::string, VarType> var_types;
    std::set<std::string> defined_funcs;
    std::set<std::string> const_vars;
    std::map<std::string, std::vector<VarType>> func_param_types;
    // struct support
    std::map<std::string, std::map<std::string, std::string>> struct_field_ptrs;
    std::map<std::string, std::string> struct_var_types;
    std::string this_var;  // current 'this' variable name during struct init
    bool terminated;
    std::string current_function;
    // struct param/return support
    std::map<std::string, std::vector<ParamInfo>> func_param_info;
    std::map<std::string, std::string> func_return_struct;  // func -> return struct name
    std::set<std::string> func_return_class;               // func -> returns class
    std::vector<std::string> sret_field_ptrs;  // ptr regs for sret params
    std::string current_ret_struct;            // struct name if current func returns struct
    bool current_ret_is_class = false;         // true if current func returns class
    // nested struct support: var -> (dotted-path -> struct_name)
    std::map<std::string, std::map<std::string, std::string>> struct_subfield_types;
    // C header import: function name -> signature (populated by ImportCHeader::llvm_emit)
    std::map<std::string, CImportedFunc> c_imported_funcs;
    // Array variable name -> raw alloca ptr register (for provenance-safe pointer passing)
    std::map<std::string, std::string> array_data_ptrs;
    // Array variable name -> declared element type
    std::map<std::string, VarType> array_elem_types;
    // Class support
    std::map<std::string, std::string> class_var_types;
    std::map<std::string, bool> class_var_nullable;
    std::string this_class;
    std::string this_ptr_reg;
    bool classes_emitted = false;

    // Scope management for class reference counting (ARC)
    struct Scope {
        std::vector<std::string> class_var_ptrs;
    };
    std::vector<Scope> scopes;

    void push_scope() {
        scopes.emplace_back();
    }

    void register_class_var(const std::string& addr_ptr) {
        if (scopes.empty()) {
            push_scope();
        }
        scopes.back().class_var_ptrs.push_back(addr_ptr);
    }

    void emit_release_scope(const Scope& scope);
    void pop_scope();
    void emit_release_all_scopes();

    explicit LLVMGenCtx(std::ostream& o)
        : out(o), counter(0), terminated(false), classes_emitted(false) {}

    std::string fresh(const std::string& prefix = "t") {
        return "%" + prefix + "." + std::to_string(counter++);
    }

    std::string freshLabel(const std::string& prefix = "bb") {
        return prefix + "." + std::to_string(counter++);
    }

    void startBlock(const std::string& label) {
        out << "\n" << label << ":\n";
        terminated = false;
    }
};

