/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include "../include/cpu.hpp"

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
    if (argc > 0) {
        string str;
        ifstream ifs(argv[1]);
        while (!ifs.fail()) {
            string line;
            getline(ifs, line);
            str += line;
        }
        Lexer lexer;
        Parser parser;
        auto tokens = lexer.lex(str.c_str());
        auto program = parser.parse(tokens);

        map<string, int> vars;
        map<string, int> functions;
        std::vector<Code> codes;
        for (auto function : program) {
            function->compile(codes, vars, functions, 4);
        }
        codes.insert(codes.begin(),
                     Code::makeCode(Code::CALL, functions["main"], 0));
        codes.insert(codes.begin() + 1, Code::makeCode(Code::PUSHR, 2, 0));
        codes.insert(codes.begin() + 2, Code::makeCode(Code::POP, 2, 0));
        codes.insert(codes.begin() + 3, Code::makeCode(Code::EXIT, 0, 0));
        
        CPU cpu;
        cpu.add(print);
        cpu.add(put);
        cpu.add(nl);
        cpu.add(input);
        cpu.set(codes);
        return cpu.exe();
    }
    return 0;
}
