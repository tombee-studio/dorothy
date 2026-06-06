/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/code.hpp"

static std::string get_mnemonic(struct Code);

void Code::print(std::ostream& os) {
    char str[64];
    snprintf(str, sizeof(str), "%8s %5d %5d",
             get_mnemonic(*this).c_str(), op1, op2);
    os << str;
}

std::string get_mnemonic(Code code) {
    switch (code.mnemonic) {
        case Code::PUSHI:
            return "PUSHI";
        case Code::PUSHR:
            return "PUSHR";
        case Code::POP:
            return "POP";
        case Code::ADD:
            return "ADD";
        case Code::SUB:
            return "SUB";
        case Code::MUL:
            return "MUL";
        case Code::DIV:
            return "DIV";
        case Code::MOD:
            return "MOD";
        case Code::JMP:
            return "JMP";
        case Code::JNE:
            return "JNE";
        case Code::JE:
            return "JE";
        case Code::LE:
            return "LE";
        case Code::LT:
            return "LT";
        case Code::GE:
            return "GE";
        case Code::GT:
            return "GT";
        case Code::EQ:
            return "EQ";
        case Code::NE:
            return "NE";
        case Code::AND:
            return "AND";
        case Code::OR:
            return "OR";
        case Code::STORE:
            return "STORE";
        case Code::LOAD:
            return "LOAD";
        case Code::CALL:
            return "CALL";
        case Code::RET:
            return "RET";
        case Code::EXIT:
            return "EXIT";
        case Code::MOVE:
            return "MOVE";
        case Code::MOVEI:
            return "MOVEI";
        case Code::INT:
            return "INT";
        case Code::FADD:
            return "FADD";
        case Code::FSUB:
            return "FSUB";
        case Code::FMUL:
            return "FMUL";
        case Code::FDIV:
            return "FDIV";
        default:
            return "";
    }
}

Code::Mnemonic Code::getCodeFromName(std::string name) {
    if (name == "PUSHI") return Code::PUSHI;
    if (name == "PUSHR") return Code::PUSHR;
    if (name == "POP") return Code::POP;
    if (name == "ADD") return Code::ADD;
    if (name == "SUB") return Code::SUB;
    if (name == "MUL") return Code::MUL;
    if (name == "DIV") return Code::DIV;
    if (name == "MOD") return Code::MOD;
    if (name == "LE") return Code::LE;
    if (name == "LT") return Code::LT;
    if (name == "GE") return Code::GE;
    if (name == "GT") return Code::GT;
    if (name == "EQ") return Code::EQ;
    if (name == "NE") return Code::NE;
    if (name == "JNE") return Code::JNE;
    if (name == "JE") return Code::JE;
    if (name == "JMP") return Code::JMP;
    if (name == "STORE") return Code::STORE;
    if (name == "LOAD") return Code::LOAD;
    if (name == "CALL") return Code::CALL;
    if (name == "RET") return Code::RET;
    if (name == "EXIT") return Code::EXIT;
    if (name == "MOVE") return Code::MOVE;
    if (name == "MOVEI") return Code::MOVEI;
    if (name == "INT") return Code::INT;
    if (name == "FADD") return Code::FADD;
    if (name == "FSUB") return Code::FSUB;
    if (name == "FMUL") return Code::FMUL;
    if (name == "FDIV") return Code::FDIV;
    fprintf(stderr, "undefined mnemonic: %s\n", name.c_str());
    exit(-1);
}

Code Code::makeCode(std::string text) {
    Code code;
    char s[128];
    int op1;
    int op2;
    sscanf(text.c_str(), "%8s %5d %5d", s, &op1, &op2);
    code.mnemonic = Code::getCodeFromName(s);
    code.op1 = op1;
    code.op2 = op2;
    return code;
}

Code Code::makeCode(Code::Mnemonic mnemonic, int op1, int op2) {
    Code code;
    code.mnemonic = mnemonic;
    code.op1 = op1;
    code.op2 = op2;
    return code;
}
