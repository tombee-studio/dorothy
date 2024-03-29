/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/cpu.hpp"

#include <fstream>
using std::cout;
using std::cin;
using std::endl;
using std::ifstream;
using std::ofstream;

int print(int sp, int *_memory) {
    cout << _memory[sp];
    return 0;
}

int nl(int sp, int *_memory) {
    cout << endl;
    return 0;
}

int put(int sp, int *_memory) {
    cout << static_cast<char>(_memory[sp]);
    return 0;
}

int input(int sp, int *_memory) {
    cin >> _memory[sp + 1];
    return 0;
}

int main(int argc, char **argv) {
    int index = 1;
    vector<Code> codes;
    ifstream ifs(argv[1]);
    char str[256];
    while (ifs.getline(str, 256 - 1)) {
        char s[128];
        int op1;
        int op2;
        sscanf(str, "%s %d %d", s, &op1, &op2);
        Code code;
        code.mnemonic = Code::getCodeFromName(s);
        code.index = index;
        code.op1 = op1;
        code.op2 = op2;
        codes.push_back(code);
        index++;
    }
    ifs.close();

    CPU cpu;
    cpu.add(print);
    cpu.add(put);
    cpu.add(nl);
    cpu.add(input);
    cpu.set(codes);
    return cpu.exe();
}
