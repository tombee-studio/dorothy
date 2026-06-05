/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

#include "../include/cpu.hpp"
#include "../include/lexer.hpp"
#include "../include/llvm_gen.hpp"
#include "../include/parser.hpp"

using std::cerr;
using std::cin;
using std::cout;
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

enum class Target { NONE, WIN, LINUX, MACOS };

static Target parse_target(const char *s) {
    string t(s);
    if (t == "win")   return Target::WIN;
    if (t == "linux") return Target::LINUX;
    if (t == "macos") return Target::MACOS;
    return Target::NONE;
}

static const char *get_triple(Target t) {
    switch (t) {
        case Target::WIN:   return "x86_64-w64-mingw32";
        case Target::LINUX: return "x86_64-pc-linux-gnu";
        case Target::MACOS: return "arm64-apple-macosx12.0";
        default:            return nullptr;
    }
}

static const char *get_exe_ext(Target t) {
    return (t == Target::WIN) ? ".exe" : "";
}

static string find_tool(const char *candidates[]) {
    const char *prefix = getenv("LLVM_PREFIX");
    if (prefix) {
        // candidates[0] の basename だけ LLVM_PREFIX 配下に探す
        string path = string(prefix) + "/bin/" + candidates[0];
        if (access(path.c_str(), X_OK) == 0) return path;
    }
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) return candidates[i];
    }
    return candidates[0];  // fall back to PATH lookup
}

static string find_llvm_clang() {
    static const char *candidates[] = {
        "/opt/homebrew/opt/llvm/bin/clang",  // macOS ARM64 (Homebrew)
        "/usr/local/opt/llvm/bin/clang",      // macOS Intel (Homebrew)
        "/usr/lib/llvm/bin/clang",            // Arch Linux
        "/usr/bin/clang",                      // Ubuntu / Fedora / RHEL
        "/usr/local/bin/clang",               // manual install
        nullptr,
    };
    const char *prefix = getenv("LLVM_PREFIX");
    if (prefix) {
        string path = string(prefix) + "/bin/clang";
        if (access(path.c_str(), X_OK) == 0) return path;
    }
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) return candidates[i];
    }
    return "clang";
}

static string find_llc() {
    const char *prefix = getenv("LLVM_PREFIX");
    if (prefix) {
        string path = string(prefix) + "/bin/llc";
        if (access(path.c_str(), X_OK) == 0) return path;
    }
    static const char *candidates[] = {
        "/opt/homebrew/opt/llvm/bin/llc",  // macOS ARM64 (Homebrew)
        "/usr/local/opt/llvm/bin/llc",      // macOS Intel (Homebrew)
        "/usr/lib/llvm/bin/llc",            // Arch Linux
        "/usr/bin/llc",                      // Ubuntu / Fedora / RHEL
        "/usr/local/bin/llc",               // manual install
        nullptr,
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) return candidates[i];
    }
    return "llc";
}

static string find_mingw_gcc() {
    static const char *candidates[] = {
        "/opt/homebrew/bin/x86_64-w64-mingw32-gcc",  // macOS (Homebrew)
        "/usr/bin/x86_64-w64-mingw32-gcc",             // Ubuntu / Debian
        "/usr/local/bin/x86_64-w64-mingw32-gcc",       // manual install
        nullptr,
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) return candidates[i];
    }
    return "x86_64-w64-mingw32-gcc";
}

static string file_stem(const string &path) {
    size_t slash = path.rfind('/');
    size_t dot   = path.rfind('.');
    size_t start = (slash == string::npos) ? 0 : slash + 1;
    if (dot != string::npos && dot > start)
        return path.substr(start, dot - start);
    return path.substr(start);
}

static int compile_windows(const string &ir_file, const string &out_file) {
    string obj_file = ir_file + ".obj";

    // Step 1: IR -> COFF object via llc
    string llc_cmd = find_llc() +
                     " --mtriple=x86_64-w64-mingw32" +
                     " -filetype=obj" +
                     " -o " + obj_file +
                     " " + ir_file;
    if (system(llc_cmd.c_str()) != 0) {
        remove(obj_file.c_str());
        return 1;
    }

    // Step 2: COFF object -> .exe via MinGW-w64 GCC
    string gcc_cmd = find_mingw_gcc() +
                     " -o " + out_file +
                     " " + obj_file;
    int ret = system(gcc_cmd.c_str());
    remove(obj_file.c_str());
    return ret == 0 ? 0 : 1;
}

static int compile_unix(const string &ir_file, const string &out_file,
                         Target target) {
    string cmd = find_llvm_clang() +
                 " --target=" + get_triple(target) +
                 " -Wno-override-module" +
                 " -o " + out_file +
                 " " + ir_file;
    return system(cmd.c_str()) == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    bool emit_llvm = false;
    Target target = Target::NONE;
    const char *input_file = nullptr;

    for (int i = 1; i < argc; i++) {
        string arg(argv[i]);
        if (arg == "--emit-llvm") {
            emit_llvm = true;
        } else if (arg == "--target" || arg == "-t") {
            if (i + 1 >= argc) {
                cerr << "error: --target requires an argument (win|linux|macos)" << endl;
                return 1;
            }
            target = parse_target(argv[++i]);
            if (target == Target::NONE) {
                cerr << "error: unknown target '" << argv[i]
                     << "' (use: win, linux, macos)" << endl;
                return 1;
            }
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
    auto tokens  = lexer.lex(str.c_str());
    auto program = parser.parse(tokens);

    if (target != Target::NONE) {
        string tmp_file = string("/tmp/dorothy_") + file_stem(input_file) + ".ll";
        {
            ofstream tmp(tmp_file);
            tmp << "target triple = \"" << get_triple(target) << "\"\n\n";
            LLVMGenCtx ctx(tmp);
            for (auto func : program) {
                if (!func->isImport())
                    ctx.defined_funcs.insert(func->getName());
            }
            for (auto func : program) {
                func->llvm_emit(ctx);
            }
        }

        string out_file = file_stem(input_file) + get_exe_ext(target);
        int ret = (target == Target::WIN)
                  ? compile_windows(tmp_file, out_file)
                  : compile_unix(tmp_file, out_file, target);
        remove(tmp_file.c_str());
        if (ret == 0) {
            cout << "output: " << out_file << endl;
        }
        return ret;
    }

    if (emit_llvm) {
        LLVMGenCtx ctx(cout);
        for (auto func : program) {
            if (!func->isImport())
                ctx.defined_funcs.insert(func->getName());
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
