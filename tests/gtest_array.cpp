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

    char tmp_ll[] = "./_tmp_array_XXXXXX.ll";
    char tmp_bin[] = "./_tmp_array_bin_XXXXXX";
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

    std::string compile_cmd = "clang -Wno-override-module -o " + std::string(tmp_bin) + " " + std::string(tmp_ll) + " 2>&1";
    int compile_res = system(compile_cmd.c_str());
    if (compile_res != 0) {
        unlink(tmp_ll);
        unlink(tmp_bin);
        throw std::runtime_error("clang compilation failed");
    }

    int exit_code = system(tmp_bin);
    int ret = WEXITSTATUS(exit_code);
    unlink(tmp_ll);
    unlink(tmp_bin);
    return ret;
}

static std::string run_llvm_stdout(const std::string& source) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(source);
    auto program = parser.parse(tokens);

    char tmp_ll[] = "./_tmp_array_out_XXXXXX.ll";
    char tmp_bin[] = "./_tmp_array_out_bin_XXXXXX";
    char tmp_out[] = "./_tmp_array_out_txt_XXXXXX";
    int fd_ll = mkstemps(tmp_ll, 3);
    int fd_bin = mkstemp(tmp_bin);
    int fd_out = mkstemp(tmp_out);
    if (fd_ll >= 0) close(fd_ll);
    if (fd_bin >= 0) close(fd_bin);
    if (fd_out >= 0) close(fd_out);

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

    std::string compile_cmd = "clang -Wno-override-module -o " + std::string(tmp_bin) + " " + std::string(tmp_ll) + " 2>&1";
    int compile_res = system(compile_cmd.c_str());
    if (compile_res != 0) {
        unlink(tmp_ll);
        unlink(tmp_bin);
        unlink(tmp_out);
        throw std::runtime_error("clang compilation failed");
    }

    std::string run_cmd = std::string(tmp_bin) + " > " + std::string(tmp_out) + " 2>&1";
    system(run_cmd.c_str());

    std::ifstream ifs(tmp_out);
    std::string output((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    unlink(tmp_ll);
    unlink(tmp_bin);
    unlink(tmp_out);
    return output;
}

// ===== Parser Tests =====

TEST(ArrayTest, ParseTypeIntArray) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var arr: int[] = []; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ArrayTest, ParseTypeGenericArray) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var arr: Array<int> = []; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ArrayTest, ParseArrayLiteralElements) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { let arr = [10, 20, 30]; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
}

// ===== LLVM Code Generation & Execution Tests =====

TEST(ArrayTest, BasicPushAndIndexAccess) {
    std::string src = R"(
func main() -> int {
    var arr: int[] = [];
    arr.push(10);
    arr.push(20);
    arr.push(30);
    return arr[1];
}
)";
    EXPECT_EQ(run_llvm(src), 20);
}

TEST(ArrayTest, ArrayLengthProperty) {
    std::string src = R"(
func main() -> int {
    var arr: Array<int> = [1, 2, 3, 4, 5];
    return arr.length;
}
)";
    EXPECT_EQ(run_llvm(src), 5);
}

TEST(ArrayTest, ArrayIndexAssignment) {
    std::string src = R"(
func main() -> int {
    var arr: int[] = [10, 20, 30];
    arr[1] = 99;
    return arr[1];
}
)";
    EXPECT_EQ(run_llvm(src), 99);
}

TEST(ArrayTest, ArrayRemoveValueType) {
    std::string src = R"(
func main() -> int {
    var arr: int[] = [10, 20, 30, 40];
    arr.remove(20);
    if (arr.length != 3) {
        return 1;
    }
    if (arr[0] != 10) return 2;
    if (arr[1] != 30) return 3;
    if (arr[2] != 40) return 4;
    return 0;
}
)";
    EXPECT_EQ(run_llvm(src), 0);
}

TEST(ArrayTest, ArrayRemoveReferenceType) {
    std::string src = R"(
class Player {
    var id: int;
    func Player(id: int) {
        this.id = id;
    }
}

func main() -> int {
    var p1: Player = Player(101);
    var p2: Player = Player(102);
    var p3: Player = Player(103);
    var list: Array<Player> = [p1, p2, p3];

    list.remove(p2);

    if (list.length != 2) return 1;
    var first: Player = list[0];
    var second: Player = list[1];
    if (first.id != 101) return 2;
    if (second.id != 103) return 3;
    return 0;
}
)";
    EXPECT_EQ(run_llvm(src), 0);
}

TEST(ArrayTest, ArrayLoopSum) {
    std::string src = R"(
func main() -> int {
    var arr: int[] = [1, 2, 3, 4, 5];
    var sum: int = 0;
    var i: int = 0;
    for (i = 0; i < arr.length; i = i + 1) {
        sum = sum + arr[i];
    }
    return sum;
}
)";
    EXPECT_EQ(run_llvm(src), 15);
}

TEST(ArrayTest, ArrayStringElements) {
    std::string src = R"(
import "stdio.h";

func main() -> int {
    var names: string[] = ["Alice", "Bob", "Charlie"];
    names.push("Dorothy");
    printf("%s %s\n", names[0], names[3]);
    return 0;
}
)";
    EXPECT_EQ(run_llvm_stdout(src), "Alice Dorothy\n");
}
