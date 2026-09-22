/* Copyright 2026(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

#include "../include/ast.hpp"
#include "../include/lexer.hpp"
#include "../include/llvm_gen.hpp"
#include "../include/parser.hpp"
#include "../include/typechecker.hpp"

static void clear_global_defs() {
    g_struct_defs.clear();
    g_class_defs.clear();
    g_var_struct_types.clear();
    g_var_class_types.clear();
    g_this_struct.clear();
    g_this_class.clear();
}

static std::string llvm_emit_ir(const std::string& source) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(source);
    auto program = parser.parse(tokens);

    std::ostringstream oss;
    LLVMGenCtx ctx(oss);
    for (auto func : program) {
        if (!func->isImport())
            ctx.defined_funcs.insert(func->getName());
    }
    for (auto func : program) {
        func->llvm_pre_register(ctx);
    }
    for (auto func : program) {
        func->llvm_emit(ctx);
    }
    return oss.str();
}

static int run_llvm(const std::string& source) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(source);
    auto program = parser.parse(tokens);

    char tmp_ll[] = "/tmp/dorothy_test_str_XXXXXX.ll";
    char tmp_bin[] = "/tmp/dorothy_test_str_bin_XXXXXX";
    int fd_ll = mkstemps(tmp_ll, 3);
    int fd_bin = mkstemp(tmp_bin);
    if (fd_ll >= 0) close(fd_ll);
    if (fd_bin >= 0) close(fd_bin);

    std::ofstream ofs(tmp_ll);
    LLVMGenCtx ctx(ofs);
    for (auto func : program) {
        if (!func->isImport())
            ctx.defined_funcs.insert(func->getName());
    }
    for (auto func : program) {
        func->llvm_pre_register(ctx);
    }
    for (auto func : program) {
        func->llvm_emit(ctx);
    }
    ofs.close();

    std::string err_file = std::string(tmp_bin) + ".err";
    std::string cmd = "clang -Wno-override-module -o " + std::string(tmp_bin) + " " + std::string(tmp_ll) + " >" + err_file + " 2>&1";
    int compile_res = system(cmd.c_str());
    if (compile_res != 0) {
        std::ifstream efs(err_file);
        std::string err_str((std::istreambuf_iterator<char>(efs)), std::istreambuf_iterator<char>());
        std::ifstream lfs(tmp_ll);
        std::string ll_str((std::istreambuf_iterator<char>(lfs)), std::istreambuf_iterator<char>());
        remove(tmp_ll);
        remove(tmp_bin);
        remove(err_file.c_str());
        throw std::runtime_error("clang compilation failed:\n" + err_str + "\nGenerated LLVM IR:\n" + ll_str);
    }
    remove(err_file.c_str());

    int exec_res = system(tmp_bin);
    int exit_code = WEXITSTATUS(exec_res);

    remove(tmp_ll);
    remove(tmp_bin);
    return exit_code;
}

// =======================================================
// 1. Lexer tests
// =======================================================

TEST(StringTest, LexerKeywordAndLiteral) {
    Lexer lexer;
    auto tokens = lexer.lex("var s: string = \"hello world\";");
    ASSERT_GE(tokens.size(), 7u);
    EXPECT_EQ(tokens[0].type, Token::KW_VAR);
    EXPECT_EQ(tokens[1].type, Token::TK_ID);
    EXPECT_EQ(tokens[1].id, "s");
    EXPECT_EQ(tokens[2].type, (Token::Type)':');
    EXPECT_EQ(tokens[3].type, Token::KW_STRING);
    EXPECT_EQ(tokens[4].type, (Token::Type)'=');
    EXPECT_EQ(tokens[5].type, Token::TK_RAWSTRING);
    EXPECT_EQ(tokens[5].id, "hello world");
}

// =======================================================
// 2. Parser tests
// =======================================================

TEST(StringTest, ParseStringDeclaration) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func main(): int {\n"
        "    var msg: string = \"dorothy\";\n"
        "    return 0;\n"
        "}"
    );
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1);
    EXPECT_EQ(program[0]->getName(), "main");
    EXPECT_EQ(program[0]->getRetType(), VarType::INT);
}

// =======================================================
// 3. Typechecker tests
// =======================================================

TEST(StringTest, TypecheckerValidString) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func main(): int {\n"
        "    var a: string = \"hello\";\n"
        "    var b: string = \"world\";\n"
        "    var c: string = a + b;\n"
        "    return 0;\n"
        "}"
    );
    auto program = parser.parse(tokens);
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(program));
}

TEST(StringTest, TypecheckerRejectIntToString) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func main(): int {\n"
        "    var a: string = 123;\n"
        "    return 0;\n"
        "}"
    );
    auto program = parser.parse(tokens);
    TypeChecker tc;
    EXPECT_THROW(tc.check(program), TypeCheckError);
}

// =======================================================
// 4. LLVM IR generation & Execution tests
// =======================================================

TEST(StringTest, LLVMStringLiteralAndVariable) {
    std::string src =
        "func main(): int {\n"
        "    var s: string = \"apple\";\n"
        "    if (s == \"apple\") {\n"
        "        return 10;\n"
        "    }\n"
        "    return 20;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 10);
}

TEST(StringTest, LLVMStringTypeInference) {
    std::string src =
        "func main(): int {\n"
        "    var s = \"banana\";\n"
        "    if (s == \"banana\") {\n"
        "        return 42;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 42);
}

TEST(StringTest, LLVMStringReassignment) {
    std::string src =
        "func main(): int {\n"
        "    var s: string = \"one\";\n"
        "    s = \"two\";\n"
        "    if (s == \"two\") {\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 1);
}

TEST(StringTest, LLVMStringConcatenation) {
    std::string src =
        "func main(): int {\n"
        "    var a: string = \"hello \";\n"
        "    var b: string = \"world\";\n"
        "    var c: string = a + b;\n"
        "    if (c == \"hello world\") {\n"
        "        return 100;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 100);
}

TEST(StringTest, LLVMStringChainedConcatenation) {
    std::string src =
        "func main(): int {\n"
        "    var full: string = \"a\" + \"b\" + \"c\" + \"d\";\n"
        "    if (full == \"abcd\") {\n"
        "        return 7;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 7);
}

TEST(StringTest, LLVMStringComparisonNE) {
    std::string src =
        "func main(): int {\n"
        "    var a: string = \"cat\";\n"
        "    var b: string = \"dog\";\n"
        "    if (a != b) {\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 1);
}

TEST(StringTest, LLVMStringComparisonOrdering) {
    std::string src =
        "func main(): int {\n"
        "    var a: string = \"abc\";\n"
        "    var b: string = \"xyz\";\n"
        "    var ok: int = 0;\n"
        "    if (a < b) { ok = ok + 1; }\n"
        "    if (b > a) { ok = ok + 1; }\n"
        "    if (a <= \"abc\") { ok = ok + 1; }\n"
        "    if (b >= \"xyz\") { ok = ok + 1; }\n"
        "    return ok;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 4);
}

TEST(StringTest, LLVMStringFunctionParamAndReturn) {
    std::string src =
        "func greet(name: string): string {\n"
        "    return \"Hello, \" + name;\n"
        "}\n"
        "func main(): int {\n"
        "    var res: string = greet(\"Dorothy\");\n"
        "    if (res == \"Hello, Dorothy\") {\n"
        "        return 55;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 55);
}

TEST(StringTest, LLVMStringClassField) {
    std::string src =
        "class Person {\n"
        "    var name: string;\n"
        "    var age: int;\n"
        "    constructor(n: string, a: int) {\n"
        "        this.name = n;\n"
        "        this.age = a;\n"
        "    }\n"
        "    func getGreeting(): string {\n"
        "        return \"I am \" + this.name;\n"
        "    }\n"
        "}\n"
        "func main(): int {\n"
        "    var p: Person = Person(\"Alice\", 20);\n"
        "    if (p.name == \"Alice\") {\n"
        "        if (p.getGreeting() == \"I am Alice\") {\n"
        "            return 88;\n"
        "        }\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 88);
}

TEST(StringTest, LLVMStringEmptyString) {
    std::string src =
        "func main(): int {\n"
        "    var empty: string = \"\";\n"
        "    var res: string = empty + \"nonempty\";\n"
        "    if (res == \"nonempty\") {\n"
        "        if (empty != \"nonempty\") {\n"
        "            return 99;\n"
        "        }\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 99);
}
