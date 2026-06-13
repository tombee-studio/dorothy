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
    std::vector<std::string> sret_field_ptrs;  // ptr regs for sret params
    std::string current_ret_struct;            // struct name if current func returns struct
    // nested struct support: var -> (dotted-path -> struct_name)
    std::map<std::string, std::map<std::string, std::string>> struct_subfield_types;

    explicit LLVMGenCtx(std::ostream& o)
        : out(o), counter(0), terminated(false) {}

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
