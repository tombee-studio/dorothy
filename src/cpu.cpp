/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <cstring>
#include "../include/cpu.hpp"

void CPU::set(vector<Code>& codes) { _codes = codes; }

void CPU::run() {
    if (pc < _codes.size()) {
        run(_codes[pc]);
    } else {
        throw CantAccessToBadFieldException(
            "プログラムカウンタがプログラム領域を逸脱しています", *this);
    }
}

void CPU::add(int (*func)(int, int*)) { funcs.push_back(func); }

void CPU::run(Code code) {
    switch (code.mnemonic) {
        case Code::PUSHI:
            reg[SP_INDEX]--;
            _memory[reg[SP_INDEX]] = code.op1;
            break;
        case Code::PUSHR:
            if (code.op1 < REG_SIZE) {
                int value = reg[code.op1];
                reg[SP_INDEX]--;
                _memory[reg[SP_INDEX]] = value;
            } else {
                throw CantAccessToBadFieldException(
                    "POP命令で指定されたレジスタの値が不正です", *this);
            }
            break;
        case Code::POP:
            if (code.op1 < REG_SIZE) {
                reg[code.op1] = _memory[reg[SP_INDEX]];
                reg[SP_INDEX]++;
            } else {
                throw CantAccessToBadFieldException(
                    "POP命令で指定されたレジスタの値が不正です", *this);
            }
            break;
        case Code::ADD:
            reg[2] = reg[2] + reg[3];
            break;
        case Code::SUB:
            reg[2] = reg[2] - reg[3];
            break;
        case Code::MUL:
            reg[2] = reg[2] * reg[3];
            break;
        case Code::DIV:
            reg[2] = reg[2] / reg[3];
            break;
        case Code::MOD:
            reg[2] = reg[2] % reg[3];
            break;
        case Code::JE:
            if (reg[2]) pc = code.op1;
            break;
        case Code::JNE:
            if (!reg[2]) pc = code.op1;
            break;
        case Code::JMP:
            pc = code.op1;
            break;
        case Code::LE:
            reg[2] = reg[2] <= reg[3];
            break;
        case Code::LT:
            reg[2] = reg[2] < reg[3];
            break;
        case Code::GE:
            reg[2] = reg[2] >= reg[3];
            break;
        case Code::GT:
            reg[2] = reg[2] > reg[3];
            break;
        case Code::EQ:
            reg[2] = reg[2] == reg[3];
            break;
        case Code::NE:
            reg[2] = reg[2] != reg[3];
            break;
        case Code::AND:
            reg[2] = reg[2] && reg[3];
            break;
        case Code::OR:
            reg[2] = reg[2] || reg[3];
            break;
        case Code::STORE:
            _memory[reg[code.op1]] = reg[code.op2];
            break;
        case Code::LOAD:
            reg[code.op1] = _memory[reg[code.op2]];
            break;
        case Code::CALL:
            reg[SP_INDEX]--;
            _memory[reg[SP_INDEX]] = pc;
            pc = code.op1;
            break;
        case Code::RET:
            pc = _memory[reg[SP_INDEX]];
            reg[SP_INDEX]++;
            break;
        case Code::MOVE:
            reg[code.op1] = reg[code.op2];
            break;
        case Code::MOVEI:
            reg[code.op1] = code.op2;
            break;
        case Code::INT:
            _memory[reg[2]] = funcs[code.op1](reg[SP_INDEX], _memory);
            break;
        case Code::EXIT:
            _exit = true;
            break;
        case Code::FADD: {
            float a, b;
            memcpy(&a, &reg[2], sizeof(float));
            memcpy(&b, &reg[3], sizeof(float));
            float r = a + b;
            memcpy(&reg[2], &r, sizeof(float));
            break;
        }
        case Code::FSUB: {
            float a, b;
            memcpy(&a, &reg[2], sizeof(float));
            memcpy(&b, &reg[3], sizeof(float));
            float r = a - b;
            memcpy(&reg[2], &r, sizeof(float));
            break;
        }
        case Code::FMUL: {
            float a, b;
            memcpy(&a, &reg[2], sizeof(float));
            memcpy(&b, &reg[3], sizeof(float));
            float r = a * b;
            memcpy(&reg[2], &r, sizeof(float));
            break;
        }
        case Code::FDIV: {
            float a, b;
            memcpy(&a, &reg[2], sizeof(float));
            memcpy(&b, &reg[3], sizeof(float));
            float r = a / b;
            memcpy(&reg[2], &r, sizeof(float));
            break;
        }
    }
    pc++;
}

int CPU::exe() {
    while (!_exit) {
        run();
    }
    return reg[2];
}

void CPU::print(ostream& os) {
    os << "===============" << endl;
    for (int i = (pc > 5 ? pc - 5 : 0);
         i < (pc > _codes.size() - 5 ? _codes.size() - 1 : pc + 5); i++) {
        _codes[i].print(os);
        if (i == pc) {
            os << " < pc";
        }
        os << endl;
    }
    os << "---------------" << endl;
    for (int i = 0; i < REG_SIZE; i++) {
        os << i << ": " << reg[i] << " ";
    }
    os << endl;
    os << "---------------" << endl;
    for (int i = STACK_SIZE - 1; i >= reg[SP_INDEX]; i--) {
        os << _memory[i];
        if (i == reg[SP_INDEX]) {
            os << " < sp";
        }
        if (i == reg[BP_INDEX]) {
            os << " < bp";
        }
        os << endl;
    }
    os << endl << "===============";
    os << endl;
}
