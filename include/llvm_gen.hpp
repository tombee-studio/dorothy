/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once

#include <iostream>
#include <map>
#include <set>
#include <string>

struct LLVMGenCtx {
    std::ostream& out;
    int counter;
    std::map<std::string, std::string> vars;
    std::set<std::string> defined_funcs;
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
