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

    char tmp_ll[] = "/tmp/dorothy_test_const_XXXXXX.ll";
    char tmp_bin[] = "/tmp/dorothy_test_const_bin_XXXXXX";
    int fd_ll = mkstemps(tmp_ll, 3);
    int fd_bin = mkstemp(tmp_bin);
    if (fd_ll >= 0) close(fd_ll);
    if (fd_bin >= 0) close(fd_bin);

    {
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
    }

    std::string clang_cmd = "clang -O0 -Wno-override-module " + std::string(tmp_ll) + " -o " + std::string(tmp_bin);
    int clang_res = std::system(clang_cmd.c_str());
    if (clang_res != 0) {
        std::remove(tmp_ll);
        std::remove(tmp_bin);
        return -1;
    }

    int exit_status = std::system(tmp_bin);
    int ret_val = WEXITSTATUS(exit_status);

    std::remove(tmp_ll);
    std::remove(tmp_bin);
    return ret_val;
}

static std::string run_llvm_stdout(const std::string& source) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(source);
    auto program = parser.parse(tokens);

    char tmp_ll[] = "/tmp/dorothy_test_const_XXXXXX.ll";
    char tmp_bin[] = "/tmp/dorothy_test_const_bin_XXXXXX";
    char tmp_out[] = "/tmp/dorothy_test_const_out_XXXXXX";
    int fd_ll = mkstemps(tmp_ll, 3);
    int fd_bin = mkstemp(tmp_bin);
    int fd_out = mkstemp(tmp_out);
    if (fd_ll >= 0) close(fd_ll);
    if (fd_bin >= 0) close(fd_bin);
    if (fd_out >= 0) close(fd_out);

    {
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
    }

    std::string clang_cmd = "clang -O0 -Wno-override-module " + std::string(tmp_ll) + " -o " + std::string(tmp_bin);
    int clang_res = std::system(clang_cmd.c_str());
    if (clang_res != 0) {
        std::remove(tmp_ll);
        std::remove(tmp_bin);
        std::remove(tmp_out);
        return "";
    }

    std::string run_cmd = std::string(tmp_bin) + " > " + std::string(tmp_out);
    std::system(run_cmd.c_str());

    std::ifstream ifs(tmp_out);
    std::string result((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    std::remove(tmp_ll);
    std::remove(tmp_bin);
    std::remove(tmp_out);
    return result;
}

// 1. Lexer: Token::KW_LET
TEST(ConstantsTest, LexerKeywordLet) {
    Lexer lexer;
    auto tokens = lexer.lex("let x = 10;");
    ASSERT_GE(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].type, Token::KW_LET);
    EXPECT_EQ(tokens[1].type, Token::TK_ID);
    EXPECT_EQ(tokens[1].id, "x");
}

// 2. Parser: Explicit type let
TEST(ConstantsTest, ParseExplicitTypeLet) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { let a: int = 10; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    auto* block = dynamic_cast<Block*>(program[0]->getBody());
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->getStatements().size(), 1u);
    auto* decl_st = dynamic_cast<DeclVarSt*>(block->getStatements()[0]);
    ASSERT_NE(decl_st, nullptr);
    EXPECT_TRUE(decl_st->getDecl()->isConst());
    EXPECT_EQ(decl_st->getDecl()->getId(), "a");
    EXPECT_EQ(decl_st->getDecl()->getType(), VarType::INT);
}

// 3. Parser: Inferred type let
TEST(ConstantsTest, ParseInferredTypeLet) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { let a = 10; let s = \"dorothy\"; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    auto* block = dynamic_cast<Block*>(program[0]->getBody());
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->getStatements().size(), 2u);

    auto* decl_st0 = dynamic_cast<DeclVarSt*>(block->getStatements()[0]);
    ASSERT_NE(decl_st0, nullptr);
    EXPECT_TRUE(decl_st0->getDecl()->isConst());
    EXPECT_EQ(decl_st0->getDecl()->getId(), "a");

    auto* decl_st1 = dynamic_cast<DeclVarSt*>(block->getStatements()[1]);
    ASSERT_NE(decl_st1, nullptr);
    EXPECT_TRUE(decl_st1->getDecl()->isConst());
    EXPECT_EQ(decl_st1->getDecl()->getId(), "s");
    EXPECT_EQ(decl_st1->getDecl()->getType(), VarType::STRING);
}

// 4. Parser: Uninitialized let throws ParseError
TEST(ConstantsTest, ParseUninitializedLetThrows) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens1 = lexer.lex("func main() { let a: int; }");
    EXPECT_THROW(parser.parse(tokens1), ParseError);

    clear_global_defs();
    auto tokens2 = lexer.lex("func main() { let a: int[5]; }");
    EXPECT_THROW(parser.parse(tokens2), ParseError);
}

// 5. TypeChecker: Valid let usage passes
TEST(ConstantsTest, TypecheckerValidLet) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    TypeChecker tc;
    auto tokens = lexer.lex("func main() { let a: int = 10; var b: int = a + 5; }");
    auto program = parser.parse(tokens);
    EXPECT_NO_THROW(tc.check(program));
}

// 6. TypeChecker: Reassigning let throws TypeCheckError
TEST(ConstantsTest, TypecheckerReassignLetThrows) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    TypeChecker tc;
    auto tokens = lexer.lex("func main() { let a: int = 10; a = 20; }");
    auto program = parser.parse(tokens);
    EXPECT_THROW(tc.check(program), TypeCheckError);
}

// 7. TypeChecker: Reassigning inferred let throws TypeCheckError
TEST(ConstantsTest, TypecheckerReassignInferredLetThrows) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    TypeChecker tc;
    auto tokens = lexer.lex("func main() { let a = 10; a = 2; }");
    auto program = parser.parse(tokens);
    EXPECT_THROW(tc.check(program), TypeCheckError);
}

// 8. TypeChecker: Reassigning const parameter throws TypeCheckError
TEST(ConstantsTest, TypecheckerReassignConstParamThrows) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    TypeChecker tc;
    auto tokens = lexer.lex("func foo(let x: int) { x = 10; }");
    auto program = parser.parse(tokens);
    EXPECT_THROW(tc.check(program), TypeCheckError);
}

// 9. LLVM: Read and compute with let constants
TEST(ConstantsTest, LLVMConstBasicExecution) {
    std::string src = R"(
        func main(): int {
            let a: int = 15;
            let b: int = 25;
            let c: int = a + b;
            return c;
        }
    )";
    EXPECT_EQ(run_llvm(src), 40);
}

// 10. LLVM: Inferred type let constant
TEST(ConstantsTest, LLVMConstInferredExecution) {
    std::string src = R"(
        func main(): int {
            let a = 100;
            let b = 20;
            let c = 7;
            return a + b + c;
        }
    )";
    EXPECT_EQ(run_llvm(src), 127);
}

// 11. LLVM: String let constant
TEST(ConstantsTest, LLVMConstStringExecution) {
    std::string src = R"(
        import "stdio.h";
        func main() {
            let s: string = "hello world";
            printf("%s\n", s);
        }
    )";
    EXPECT_EQ(run_llvm_stdout(src), "hello world\n");
}

// 12. LLVM: Reassigning let constant in LLVM code generation throws CompileError
TEST(ConstantsTest, LLVMConstReassignmentThrowsCompileError) {
    std::string src = R"(
        func main() {
            let a: int = 10;
            a = 2;
        }
    )";
    EXPECT_THROW(llvm_emit_ir(src), CompileError);
}

// 13. LLVM: Reassigning inferred let constant throws CompileError
TEST(ConstantsTest, LLVMInferredConstReassignmentThrowsCompileError) {
    std::string src = R"(
        func main() {
            let a = 10;
            a = 2;
        }
    )";
    EXPECT_THROW(llvm_emit_ir(src), CompileError);
}

// 14. LLVM: Class and Struct with let constant
TEST(ConstantsTest, LLVMConstClassAndStruct) {
    std::string src = R"(
        import "stdio.h";
        struct Point {
            var x: int;
            var y: int;
        }
        class Greeter {
            var prefix: string;
            func Greeter(p: string) {
                this.prefix = p;
            }
            func greet(name: string) {
                printf("%s %s\n", this.prefix, name);
            }
        }
        func main() {
            let pt: Point = Point(x=10, y=20);
            let g: Greeter = Greeter("Hello");
            g.greet("Dorothy");
        }
    )";
    EXPECT_EQ(run_llvm_stdout(src), "Hello Dorothy\n");
}
