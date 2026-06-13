/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <cstring>
#include "../include/ast.hpp"

// ===== Global struct registry =====
map<string, StructDefInfo> g_struct_defs;
string g_this_struct;
map<string, string> g_var_struct_types;

void Node::addTab(ostream& os, int tab) {
    for (int i = 0; i < tab; i++) {
        os << "  ";
    }
}

void Expression::print(ostream& os, int tab) { os << "<EXP>"; }

static const char* vartype_name(VarType t) {
    switch (t) {
        case VarType::CHAR:   return "char";
        case VarType::INT:    return "int";
        case VarType::LONG:   return "long";
        case VarType::FLOAT:  return "float";
        case VarType::DOUBLE: return "double";
    }
    return "int";
}

void DeclVar::print(ostream& os, int tab) { os << vartype_name(_type) << " " << _id; }

void DeclVar::compile(vector<Code>& ofs, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    if (!vars[_id]) {
        if (_type == VarType::STRUCT) {
            // Allocate N consecutive slots for struct fields
            auto it = g_struct_defs.find(_struct_name);
            if (it == g_struct_defs.end())
                throw CompileError(("undefined struct type: " + _struct_name).c_str());
            const auto& sdef = it->second;
            int n = (int)sdef.fields.size();
            int base = vars["."] + 1;
            vars["."] += n;
            vars[_id] = base;
            vars["$t:" + _id] = (int)VarType::STRUCT;
            if (_is_const) vars["$const:" + _id] = 1;
            g_var_struct_types[_id] = _struct_name;
            // Zero-initialize all field slots and update SP
            for (int i = 0; i < n; i++) {
                int slot = base + i;
                ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
                ofs.push_back(Code::makeCode(Code::PUSHI, slot, 0));
                ofs.push_back(Code::makeCode(Code::POP, 3, 0));
                ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
                ofs.push_back(Code::makeCode(Code::MOVEI, 3, 0));
                ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
            }
            ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
            ofs.push_back(Code::makeCode(Code::PUSHI, vars["."], 0));
            ofs.push_back(Code::makeCode(Code::POP, 3, 0));
            ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
            ofs.push_back(Code::makeCode(Code::MOVE, 1, 2));
            return;
        }
        vars["."]++;
        vars[_id] = vars["."];
        vars["$t:" + _id] = (int)_type;
        if (_is_const) vars["$const:" + _id] = 1;
        ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
        ofs.push_back(Code::makeCode(Code::PUSHI, vars[_id], 0));
        ofs.push_back(Code::makeCode(Code::POP, 3, 0));
        ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
        ofs.push_back(Code::makeCode(Code::MOVE, 1, 2));
        // Zero-initialize the slot (r2 = address of variable)
        ofs.push_back(Code::makeCode(Code::MOVEI, 3, 0));
        ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
        return;
    }
    throw CompileError((string("redeclared variable: ") + _id).c_str());
}

void InitializedDeclVar::print(ostream& os, int tab) {
    os << (_is_const ? "let" : "var") << " " << _id << ": "
       << vartype_name(_type) << " = ";
    _init->print(os, tab);
}

void InitializedDeclVar::compile(vector<Code>& ofs, map<string, int>& vars,
                                 map<string, int>& functions, int offset) {
    DeclVar::compile(ofs, vars, functions, offset);  // allocate slots + zero-init

    if (_type == VarType::STRUCT) {
        // struct-to-struct copy: var a: P = b;
        const string& raw_src = _init->getVarName();
        string src_var = (raw_src == "$this" && !g_this_struct.empty()) ? "$this" : raw_src;
        if (!src_var.empty() && g_var_struct_types.count(src_var)) {
            const string& src_struct = g_var_struct_types[src_var];
            if (src_struct != _struct_name)
                throw CompileError(("cannot copy struct '" + src_struct +
                                    "' into variable of type '" + _struct_name + "'").c_str());
            const auto& sdef = g_struct_defs[_struct_name];
            int src_base = vars.at(src_var);
            int dst_base = vars.at(_id);
            for (int fi = 0; fi < (int)sdef.fields.size(); fi++) {
                int src_slot = src_base + fi;
                int dst_slot = dst_base + fi;
                // load from src field slot
                ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
                ofs.push_back(Code::makeCode(Code::PUSHI, src_slot, 0));
                ofs.push_back(Code::makeCode(Code::POP, 3, 0));
                ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
                ofs.push_back(Code::makeCode(Code::LOAD, 2, 2));
                ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
                // store to dst field slot
                ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
                ofs.push_back(Code::makeCode(Code::PUSHI, dst_slot, 0));
                ofs.push_back(Code::makeCode(Code::POP, 3, 0));
                ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
                ofs.push_back(Code::makeCode(Code::POP, 3, 0));
                ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
            }
            return;
        }

        auto si = dynamic_cast<StructInit*>(_init);
        if (!si)
            throw CompileError("struct variable must be initialized with struct literal or another struct variable");
        const auto& sdef = g_struct_defs[_struct_name];

        // Set up 'this' context
        int before_params = vars["."];
        vars["$this"] = vars[_id];
        g_var_struct_types["$this"] = _struct_name;
        string prev_this_struct = g_this_struct;
        g_this_struct = _struct_name;

        // Allocate constructor param slots and assign arg values (positional)
        const auto& ctor = sdef.constructor;
        if (ctor) {
            const auto& call_args = si->getArgs();
            for (int pi = 0; pi < (int)ctor->params.size(); pi++) {
                const string& pname = ctor->params[pi].first;
                VarType ptype = ctor->params[pi].second;
                vars["."]++;
                int pslot = vars["."];
                vars[pname] = pslot;
                vars["$t:" + pname] = (int)ptype;
                ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
                ofs.push_back(Code::makeCode(Code::PUSHI, pslot, 0));
                ofs.push_back(Code::makeCode(Code::POP, 3, 0));
                ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
                ofs.push_back(Code::makeCode(Code::MOVE, 1, 2));
                ofs.push_back(Code::makeCode(Code::MOVEI, 3, 0));
                ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
                // Store positional arg value
                if (pi < (int)call_args.size()) {
                    call_args[pi]->compile(ofs, vars, functions, offset);
                    ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
                    ofs.push_back(Code::makeCode(Code::PUSHI, pslot, 0));
                    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
                    ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
                    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
                    ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
                }
            }
            // Execute constructor body inline
            ctor->body->compile(ofs, vars, functions, offset);

            // Clean up: reset SP to before params, remove param vars
            for (auto& [pname, ptype] : ctor->params) {
                vars.erase(pname);
                vars.erase("$t:" + pname);
            }
            vars["."] = before_params;
            ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
            ofs.push_back(Code::makeCode(Code::PUSHI, before_params, 0));
            ofs.push_back(Code::makeCode(Code::POP, 3, 0));
            ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
            ofs.push_back(Code::makeCode(Code::MOVE, 1, 2));
        }

        // Clean up 'this' context
        vars.erase("$this");
        g_var_struct_types.erase("$this");
        g_this_struct = prev_this_struct;
        return;
    }

    // Primitive: push address, compile init, store
    ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
    ofs.push_back(Code::makeCode(Code::PUSHI, vars[_id], 0));
    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
    ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
    ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
    _init->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
}

void DeclArrayVar::print(ostream& os, int tab) {
    DeclVar::print(os, tab);
    os << "[" << _num << "]";
}

void DeclArrayVar::compile(vector<Code>& ofs, map<string, int>& vars,
                           map<string, int>& functions, int offset) {
    if (!vars[_id]) {
        DeclVar::compile(ofs, vars, functions, offset);
        ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
        ofs.push_back(Code::makeCode(Code::PUSHI, _num, 0));
        ofs.push_back(Code::makeCode(Code::POP, 3, 0));
        ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
        ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
        ofs.push_back(Code::makeCode(Code::POP, 3, 0));
        ofs.push_back(Code::makeCode(Code::POP, 2, 0));
        ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
        ofs.push_back(Code::makeCode(Code::MOVE, 1, 3));
        vars["."] += _num;
        return;
    }
    throw CompileError((string("redeclared variable: ") + _id).c_str());
}

void InitializedDeclArrayVar::print(ostream& os, int tab) {
    DeclVar::print(os, tab);
    os << "[" << _num << "]={";
    for (auto it = _values.begin(); it != _values.end(); ++it) {
        (*it)->print(os, tab);
        if (it + 1 != _values.end()) {
            os << ',';
        }
    }
    os << "}";
}

void InitializedDeclArrayVar::compile(vector<Code>& ofs, map<string, int>& vars,
                                      map<string, int>& functions, int offset) {
    if (!vars[_id]) {
        DeclVar::compile(ofs, vars, functions, offset);
        ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
        ofs.push_back(Code::makeCode(Code::PUSHI, _num, 0));
        ofs.push_back(Code::makeCode(Code::POP, 3, 0));
        ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
        ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
        ofs.push_back(Code::makeCode(Code::POP, 3, 0));
        ofs.push_back(Code::makeCode(Code::POP, 2, 0));
        ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
        ofs.push_back(Code::makeCode(Code::MOVE, 1, 3));
        for (int i = 0; i < _values.size(); i++) {
            ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
            ofs.push_back(Code::makeCode(Code::PUSHI, 1, 0));
            ofs.push_back(Code::makeCode(Code::POP, 3, 0));
            ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
            ofs.push_back(Code::makeCode(Code::LOAD, 2, 2));
            ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
            ofs.push_back(Code::makeCode(Code::PUSHI, i, 0));
            ofs.push_back(Code::makeCode(Code::POP, 3, 0));
            ofs.push_back(Code::makeCode(Code::POP, 2, 0));
            ofs.push_back(Code::makeCode(Code::ADD, 0, 0));
            ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
            _values[i]->compile(ofs, vars, functions, offset);
            ofs.push_back(Code::makeCode(Code::POP, 3, 0));
            ofs.push_back(Code::makeCode(Code::POP, 2, 0));
            ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
        }
        vars["."] += _num;
        return;
    }
    throw CompileError((string("redeclared variable: ") + _id).c_str());
}

void DeclVarSt::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    _decl->print(os, tab);
    os << ";" << endl;
}

void DeclVarSt::compile(vector<Code>& ofs, map<string, int>& vars,
                        map<string, int>& functions, int offset) {
    _decl->compile(ofs, vars, functions, offset);
}

void IfSt::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    os << "if(";
    _cond->print(os, tab);
    os << ") ";
    _truest->print(os, tab);
    if (_falsest) {
        Node::addTab(os, tab);
        os << "else ";
        _falsest->print(os, tab);
    }
    os << endl;
}

void IfSt::compile(vector<Code>& ofs, map<string, int>& vars,
                   map<string, int>& functions, int offset) {
    int jumpToElse;
    int jumpFromTrue;
    _cond->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::JNE, 0, 0));
    jumpToElse = ofs.size() - 1;
    _truest->compile(ofs, vars, functions, offset);
    ofs[jumpToElse].op1 = ofs.size() + offset - 1;
    if (_falsest) {
        ofs[jumpToElse].op1 = ofs.size() + offset;
        ofs.push_back(Code::makeCode(Code::JMP, 0, 0));
        jumpFromTrue = ofs.size() - 1;
        _falsest->compile(ofs, vars, functions, offset);
        ofs[jumpFromTrue].op1 = ofs.size() - 1 + offset;
    }
}

void WhileSt::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    os << "while(";
    _cond->print(os, tab);
    os << ") ";
    _body->print(os, tab);
    os << endl;
}

void WhileSt::compile(vector<Code>& ofs, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    int jumpToBottom = 0;
    int top = ofs.size() - 1 + offset;
    _cond->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::JNE, 0, 0));
    jumpToBottom = ofs.size() - 1;
    _body->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::JMP, top, 0));
    ofs[jumpToBottom].op1 = ofs.size() - 1 + offset;
}

void ForSt::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    os << "for(";
    _init->print(os, tab);
    os << ";";
    _cond->print(os, tab);
    os << ";";
    _proceed->print(os, tab);
    os << ") ";
    _body->print(os, tab);
    os << endl;
}

void ForSt::compile(vector<Code>& ofs, map<string, int>& vars,
                    map<string, int>& functions, int offset) {
    int jumpToBottom = 0;
    _init->compile(ofs, vars, functions, offset);
    int top = ofs.size() + offset - 1;
    _cond->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::JNE, 0, 0));
    jumpToBottom = ofs.size() - 1;
    _body->compile(ofs, vars, functions, offset);
    _proceed->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::JMP, top, 0));
    ofs[jumpToBottom].op1 = ofs.size() - 1 + offset;
}

void Block::print(ostream& os, int tab) {
    os << "{" << endl;
    for (Statement* statement : _statements) {
        statement->print(os, tab + 1);
    }
    Node::addTab(os, tab);
    os << "}" << endl;
}

void Block::compile(vector<Code>& ofs, map<string, int>& vars,
                    map<string, int>& functions, int offset) {
    for (Statement* statement : _statements) {
        statement->compile(ofs, vars, functions, offset);
    }
}

void CallFuncSt::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    os << _id << "(";
    if (_args.size() > 0) {
        _args[0]->print(os, tab);
        for (int i = 1; i < _args.size(); i++) {
            os << ", ";
            _args[i]->print(os, tab);
        }
    }
    os << ")"
       << ";" << endl;
}

void CallFuncSt::compile(vector<Code>& ofs, map<string, int>& vars,
                         map<string, int>& functions, int offset) {
    if (_args.size() > 0) {
        for (auto arg : _args) {
            arg->compile(ofs, vars, functions, offset);
        }
    }
    if (functions[_id]) {
        ofs.push_back(Code::makeCode(Code::CALL, functions[_id], 0));
        if (_args.size() > 0) {
            for (auto arg : _args) {
                ofs.push_back(Code::makeCode(Code::POP, 3, 0));
            }
        }
        ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
    } else {
        throw CompileError(
            format("undefined function: %s", _id.c_str()).c_str());
    }
}

void Function::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    os << "func " << _id << "(";
    if (_args.size() > 0) {
        _args[0]->print(os, tab);
        for (int i = 1; i < _args.size(); i++) {
            os << ", ";
            _args[i]->print(os, tab);
        }
    }
    os << ")";
    _body->print(os, tab);
}

void Function::compile(vector<Code>& ofs, map<string, int>& vars,
                       map<string, int>& functions, int offset) {
    vars.clear();
    vars["."] = 0;
    g_var_struct_types.clear();
    g_this_struct.clear();
    functions[_id] = ofs.size() - 1 + offset;
    ofs.push_back(Code::makeCode(Code::PUSHR, 0, 0));
    ofs.push_back(Code::makeCode(Code::MOVE, 0, 1));
    if (_args.size() > 0) {
        for (auto arg : _args) {
            arg->compile(ofs, vars, functions, offset);
        }
        for (int i = 0; i < _args.size(); i++) {
            ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
            ofs.push_back(Code::makeCode(Code::MOVEI, 3, i + 2));
            ofs.push_back(Code::makeCode(Code::ADD, 0, 0));
            ofs.push_back(Code::makeCode(Code::LOAD, 2, 2));
            ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
            ofs.push_back(Code::makeCode(Code::MOVE, 2, 0));
            ofs.push_back(Code::makeCode(Code::MOVEI, 3, _args.size() - i));
            ofs.push_back(Code::makeCode(Code::SUB, 0, 0));
            ofs.push_back(Code::makeCode(Code::POP, 3, 0));
            ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
        }
    }
    _body->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::PUSHI, 0, 0));
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::MOVE, 1, 0));
    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
    ofs.push_back(Code::makeCode(Code::MOVE, 0, 3));
    ofs.push_back(Code::makeCode(Code::RET, 0, 0));
}

void ImportFunction::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    os << "import " << _id << endl;
}

void ImportFunction::compile(vector<Code>& ofs, map<string, int>& vars,
                             map<string, int>& functions, int offset) {
    ifstream ifs(_id + ".bin");
    int of = offset + ofs.size();
    char str[256];
    if (ifs.fail()) {
        throw CompileError(
            format("can't find 'bin' file: %s", _id.c_str()).c_str());
    }
    functions[_id] = ofs.size() - 1 + offset;
    while (ifs.getline(str, 256 - 1)) {
        char s[128];
        int op1;
        int op2;
        sscanf(str, "%s %d %d", s, &op1, &op2);
        Code code;
        code.mnemonic = Code::getCodeFromName(s);
        code.op1 = op1;
        code.op2 = op2;
        if (code.mnemonic == Code::JMP || code.mnemonic == Code::JNE ||
            code.mnemonic == Code::CALL || code.mnemonic == Code::JE) {
            code.op1 += of;
        }
        ofs.push_back(code);
    }
    ifs.close();
}

void ReturnSt::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    os << "return ";
    _exp->print(os, tab);
    os << ";" << endl;
}

void ReturnSt::compile(vector<Code>& ofs, map<string, int>& vars,
                       map<string, int>& functions, int offset) {
    _exp->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::MOVE, 1, 0));
    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
    ofs.push_back(Code::makeCode(Code::MOVE, 0, 3));
    ofs.push_back(Code::makeCode(Code::RET, 0, 0));
}

void ExpressionSt::print(ostream& os, int tab) {
    Node::addTab(os, tab);
    _exp->print(os, tab);
    os << ";" << endl;
}

void ExpressionSt::compile(vector<Code>& ofs, map<string, int>& vars,
                           map<string, int>& functions, int offset) {
    _exp->compile(ofs, vars, functions, offset);
}

void Assign::print(ostream& os, int tab) {
    _leftside->print(os, tab);
    os << "=";
    _expr->print(os, tab);
}

void Assign::compile(vector<Code>& ofs, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    const string& varname = _leftside->getVarName();
    if (!varname.empty() && vars.count("$const:" + varname))
        throw CompileError(("cannot assign to constant: " + varname).c_str());
    _leftside->lcompile(ofs, vars, functions, offset);
    _expr->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::STORE, 2, 3));
}

void Assign::lcompile(vector<Code>&, map<string, int>&, map<string, int>&,
                      int) {
    print(cerr, 0);
    throw CompileError("can't compile as left side");
}

void AddExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "+";
    _right->print(os, tab);
}

void AddExp::compile(vector<Code>& ofs, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    if (compile_type(vars) == VarType::DOUBLE) {
        ofs.push_back(Code::makeCode(Code::FADD, 0, 0));
    } else {
        ofs.push_back(Code::makeCode("ADD 0 0"));
    }
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void AddExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("ADD 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void SubExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "-";
    _right->print(os, tab);
}

void SubExp::compile(vector<Code>& ofs, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    if (compile_type(vars) == VarType::DOUBLE) {
        ofs.push_back(Code::makeCode(Code::FSUB, 0, 0));
    } else {
        ofs.push_back(Code::makeCode("SUB 0 0"));
    }
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void SubExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("SUB 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void MulExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "*";
    _right->print(os, tab);
}

void MulExp::compile(vector<Code>& ofs, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    if (compile_type(vars) == VarType::DOUBLE) {
        ofs.push_back(Code::makeCode(Code::FMUL, 0, 0));
    } else {
        ofs.push_back(Code::makeCode(Code::MUL, 0, 0));
    }
    ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
}

void MulExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("MUL 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void DivExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "/";
    _right->print(os, tab);
}

void DivExp::compile(vector<Code>& ofs, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    if (compile_type(vars) == VarType::DOUBLE) {
        ofs.push_back(Code::makeCode(Code::FDIV, 0, 0));
    } else {
        ofs.push_back(Code::makeCode("DIV 0 0"));
    }
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void DivExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("DIV 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void ModExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "%";
    _right->print(os, tab);
}

void ModExp::compile(vector<Code>& ofs, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    ofs.push_back(Code::makeCode("MOD 0 0"));
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void ModExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("MOD 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void EQExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "==";
    _right->print(os, tab);
}

void EQExp::compile(vector<Code>& ofs, map<string, int>& vars,
                    map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    ofs.push_back(Code::makeCode("EQ 0 0"));
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void EQExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("EQ 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void NEExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "!=";
    _right->print(os, tab);
}

void NEExp::compile(vector<Code>& ofs, map<string, int>& vars,
                    map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    ofs.push_back(Code::makeCode("NE 0 0"));
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void NEExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("NE 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void LTExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "<";
    _right->print(os, tab);
}

void LTExp::compile(vector<Code>& ofs, map<string, int>& vars,
                    map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    ofs.push_back(Code::makeCode("LT 0 0"));
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void LTExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("LT 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void LEExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << "<=";
    _right->print(os, tab);
}

void LEExp::compile(vector<Code>& ofs, map<string, int>& vars,
                    map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    ofs.push_back(Code::makeCode("LE 0 0"));
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void LEExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("ADD 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void GTExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << ">";
    _right->print(os, tab);
}

void GTExp::compile(vector<Code>& ofs, map<string, int>& vars,
                    map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    ofs.push_back(Code::makeCode("GT 0 0"));
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void GTExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("GT 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void GEExp::print(ostream& os, int tab) {
    _left->print(os, tab);
    os << ">=";
    _right->print(os, tab);
}

void GEExp::compile(vector<Code>& ofs, map<string, int>& vars,
                    map<string, int>& functions, int offset) {
    _left->compile(ofs, vars, functions, offset);
    _right->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode("POP 3 0"));
    ofs.push_back(Code::makeCode("POP 2 0"));
    ofs.push_back(Code::makeCode("GE 0 0"));
    ofs.push_back(Code::makeCode("PUSHR 2 0"));
}

void GEExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _left->lcompile(codes, vars, functions, offset);
    _right->lcompile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode("POP 3 0"));
    codes.push_back(Code::makeCode("POP 2 0"));
    codes.push_back(Code::makeCode("GE 0 0"));
    codes.push_back(Code::makeCode("PUSHR 2 0"));
}

void IntExp::print(ostream& os, int tab) { os << _int_val; }

void IntExp::compile(vector<Code>& ofs, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    ofs.push_back(Code::makeCode("PUSHI " + to_string(_int_val) + " 0"));
}

void IntExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    codes.push_back(Code::makeCode(Code::PUSHI, _int_val, 0));
}

void FloatExp::print(ostream& os, int tab) { os << _float_val; }

void FloatExp::compile(vector<Code>& ofs, map<string, int>& vars,
                       map<string, int>& functions, int offset) {
    float f = (float)_float_val;
    int bits;
    memcpy(&bits, &f, sizeof(float));
    ofs.push_back(Code::makeCode(Code::PUSHI, bits, 0));
}

void FloatExp::lcompile(vector<Code>& codes, map<string, int>& vars,
                        map<string, int>& functions, int offset) {
    compile(codes, vars, functions, offset);
}

void ArrayIndex::print(ostream& os, int tab) {
    _pointer->print(os, tab);
    os << "[";
    _index->print(os, tab);
    os << "]";
}

void ArrayIndex::compile(vector<Code>& codes, map<string, int>& vars,
                         map<string, int>& functions, int offset) {
    _pointer->compile(codes, vars, functions, offset);
    _index->compile(codes, vars, functions, offset);
    codes.push_back(Code::makeCode(Code::POP, 3, 0));
    codes.push_back(Code::makeCode(Code::POP, 2, 0));
    codes.push_back(Code::makeCode(Code::ADD, 0, 0));
    codes.push_back(Code::makeCode(Code::LOAD, 2, 2));
    codes.push_back(Code::makeCode(Code::PUSHR, 2, 0));
}

void ArrayIndex::lcompile(vector<Code>& ofs, map<string, int>& vars,
                          map<string, int>& functions, int offset) {
    _pointer->compile(ofs, vars, functions, offset);
    _index->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 3, 0));
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::ADD, 0, 0));
    ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
}

void Address::print(ostream& os, int tab) {
    os << "&";
    _exp->print(os, tab);
}

void Address::compile(vector<Code>& ofs, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    _exp->lcompile(ofs, vars, functions, offset);
}

void Address::lcompile(vector<Code>& ofs, map<string, int>& vars,
                       map<string, int>& functions, int offset) {
    print(cerr, 0);
    throw CompileError("can't use as left side");
}

void Access::print(ostream& os, int tab) {
    os << "*";
    _rightside->print(os, tab);
}

void Access::compile(vector<Code>& ofs, map<string, int>& vars,
                     map<string, int>& functions, int offset) {
    _rightside->compile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::LOAD, 2, 2));
    ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
}

void Access::lcompile(vector<Code>& ofs, map<string, int>& vars,
                      map<string, int>& functions, int offset) {
    _rightside->lcompile(ofs, vars, functions, offset);
    ofs.push_back(Code::makeCode(Code::POP, 2, 0));
    ofs.push_back(Code::makeCode(Code::LOAD, 2, 2));
    ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
}

void Variable::print(ostream& os, int tab) { os << _id; }

VarType Variable::llvm_declared_type(LLVMGenCtx& ctx) const {
    auto it = ctx.var_types.find(_id);
    return (it != ctx.var_types.end()) ? it->second : VarType::LONG;
}

VarType Variable::llvm_etype(LLVMGenCtx& ctx) const {
    return canonical_type(llvm_declared_type(ctx));
}

VarType Variable::compile_type(map<string, int>& vars) const {
    auto it = vars.find("$t:" + _id);
    return (it != vars.end()) ? canonical_type((VarType)it->second) : VarType::LONG;
}

void Variable::compile(vector<Code>& codes, map<string, int>& vars,
                       map<string, int>& functions, int offset) {
    try {
        codes.push_back(Code::makeCode(Code::MOVE, 2, 0));
        codes.push_back(Code::makeCode(Code::PUSHI, vars.at(_id), 0));
        codes.push_back(Code::makeCode(Code::POP, 3, 0));
        codes.push_back(Code::makeCode(Code::SUB, 0, 0));
        codes.push_back(Code::makeCode(Code::LOAD, 2, 2));
        codes.push_back(Code::makeCode(Code::PUSHR, 2, 0));
    } catch (exception& ex) {
        throw CompileError(
            format("undefined variable: %s", _id.c_str()).c_str());
    }
}

void Variable::lcompile(vector<Code>& codes, map<string, int>& vars,
                        map<string, int>& functions, int offset) {
    try {
        codes.push_back(Code::makeCode(Code::MOVE, 2, 0));
        codes.push_back(Code::makeCode(Code::PUSHI, vars.at(_id), 0));
        codes.push_back(Code::makeCode(Code::POP, 3, 0));
        codes.push_back(Code::makeCode(Code::SUB, 0, 0));
        codes.push_back(Code::makeCode(Code::PUSHR, 2, 0));
    } catch (exception& ex) {
        throw CompileError(
            format("undefined variable: %s", _id.c_str()).c_str());
    }
}

void CallFuncExp::print(ostream& os, int tab) {
    os << _id << "(";
    if (_args.size() > 0) {
        _args[0]->print(os, tab);
        for (int i = 1; i < _args.size(); i++) {
            os << ", ";
            _args[i]->print(os, tab);
        }
    }
    os << ")";
}

void CallFuncExp::compile(vector<Code>& ofs, map<string, int>& vars,
                          map<string, int>& functions, int offset) {
    if (_args.size() > 0) {
        for (auto arg : _args) {
            arg->compile(ofs, vars, functions, offset);
        }
    }
    if (functions[_id]) {
        ofs.push_back(Code::makeCode(Code::CALL, functions[_id], 0));
        if (_args.size() > 0) {
            for (auto arg : _args) {
                ofs.push_back(Code::makeCode(Code::POP, 3, 0));
            }
        }
        ofs.push_back(Code::makeCode(Code::PUSHR, 2, 0));
    } else {
        throw CompileError(
            format("undefined function: %s", _id.c_str()).c_str());
    }
}

void CallFuncExp::lcompile(vector<Code>& ofs, map<string, int>& vars,
                           map<string, int>& functions, int offset) {
    print(cerr, 0);
    throw CompileError("can't use as left side");
}

// ===== MemberAccess =====

static void member_lcompile_impl(const string& varname, const string& member,
                                  vector<Code>& codes, map<string, int>& vars) {
    auto type_it = g_var_struct_types.find(varname);
    if (type_it == g_var_struct_types.end())
        throw CompileError(("not a struct variable: " + varname).c_str());
    const string& struct_name = type_it->second;
    auto sdef_it = g_struct_defs.find(struct_name);
    if (sdef_it == g_struct_defs.end())
        throw CompileError(("unknown struct type: " + struct_name).c_str());
    int fidx = sdef_it->second.fieldIndex(member);
    if (fidx < 0)
        throw CompileError(("no field '" + member + "' in " + struct_name).c_str());
    int slot = vars.at(varname) + fidx;
    codes.push_back(Code::makeCode(Code::MOVE, 2, 0));
    codes.push_back(Code::makeCode(Code::PUSHI, slot, 0));
    codes.push_back(Code::makeCode(Code::POP, 3, 0));
    codes.push_back(Code::makeCode(Code::SUB, 0, 0));
    codes.push_back(Code::makeCode(Code::PUSHR, 2, 0));
}

void MemberAccess::print(ostream& os, int tab) {
    _object->print(os, tab);
    os << "." << _member;
}

void MemberAccess::compile(vector<Code>& codes, map<string, int>& vars,
                            map<string, int>& functions, int offset) {
    member_lcompile_impl(_object->getVarName(), _member, codes, vars);
    codes.push_back(Code::makeCode(Code::POP, 2, 0));
    codes.push_back(Code::makeCode(Code::LOAD, 2, 2));
    codes.push_back(Code::makeCode(Code::PUSHR, 2, 0));
}

void MemberAccess::lcompile(vector<Code>& codes, map<string, int>& vars,
                             map<string, int>& functions, int offset) {
    member_lcompile_impl(_object->getVarName(), _member, codes, vars);
}

VarType MemberAccess::compile_type(map<string, int>& vars) const {
    const string& varname = _object->getVarName();
    auto it = g_var_struct_types.find(varname);
    if (it == g_var_struct_types.end()) return VarType::LONG;
    const auto& sdef = g_struct_defs[it->second];
    int fidx = sdef.fieldIndex(_member);
    if (fidx < 0) return VarType::LONG;
    return canonical_type(sdef.fields[fidx].type);
}

// ===== ThisExpr =====

void ThisExpr::print(ostream& os, int tab) { os << "this"; }

void ThisExpr::compile(vector<Code>& codes, map<string, int>& vars,
                       map<string, int>& functions, int offset) {
    // 'this' alone as rval doesn't make sense; used only via MemberAccess
    throw CompileError("'this' cannot be used as a standalone expression");
}

void ThisExpr::lcompile(vector<Code>& codes, map<string, int>& vars,
                        map<string, int>& functions, int offset) {
    throw CompileError("'this' cannot be used as a standalone lvalue");
}

// ===== StructInit::print =====

void StructInit::print(ostream& os, int tab) {
    os << _struct_name << "(";
    for (int i = 0; i < (int)_args.size(); i++) {
        if (i > 0) os << ", ";
        _args[i]->print(os, tab);
    }
    os << ")";
}
