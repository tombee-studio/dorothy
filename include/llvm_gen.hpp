/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "./vartype.hpp"

struct LLVMGenCtx {
    std::ostream& out;
    int counter;
    std::map<std::string, std::string> vars;
    std::map<std::string, VarType> var_types;
    std::set<std::string> defined_funcs;
    std::set<std::string> const_vars;
    std::map<std::string, std::vector<VarType>> func_param_types;
    bool terminated;
    std::string current_function;

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
