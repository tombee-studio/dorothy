/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include "../include/cpu.hpp"
#include "../include/llvm_gen.hpp"

using std::cout;
using std::cin;
using std::endl;
using std::ifstream;
using std::ofstream;
using std::string;

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
    bool emit_llvm = false;
    const char* input_file = nullptr;

    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--emit-llvm") {
            emit_llvm = true;
        } else {
            input_file = argv[i];
        }
    }

    if (input_file == nullptr) return 1;

    string str;
    ifstream ifs(input_file);
    while (!ifs.fail()) {
        string line;
        getline(ifs, line);
        str += line;
    }

    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(str.c_str());
    auto program = parser.parse(tokens);

    if (emit_llvm) {
        LLVMGenCtx ctx(cout);
        for (auto func : program) {
            if (!func->isImport()) {
                ctx.defined_funcs.insert(func->getName());
            }
        }
        for (auto func : program) {
            func->llvm_emit(ctx);
        }
        return 0;
    }

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
