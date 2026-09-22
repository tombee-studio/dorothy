/* Copyright 2026(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

#include "../include/ast.hpp"
#include "../include/cpu.hpp"
#include "../include/lexer.hpp"
#include "../include/llvm_gen.hpp"
#include "../include/parser.hpp"

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

    char tmp_ll[] = "/tmp/dorothy_test_null_XXXXXX.ll";
    char tmp_bin[] = "/tmp/dorothy_test_null_bin_XXXXXX";
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

// ==========================================
// 1. パーサーレベル（正常系）
// ==========================================

TEST(NullSafetyTest, ParseNullableVarWithNull) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Node { var val: int; }\n"
        "func main(): int {\n"
        "    var n: Node? = null;\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_NO_THROW(parser.parse(tokens));
}

TEST(NullSafetyTest, ParseNullableVarUninit) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Node { var val: int; }\n"
        "func main(): int {\n"
        "    var n: Node?;\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_NO_THROW(parser.parse(tokens));
}

TEST(NullSafetyTest, ParseNullableFieldAndMethod) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Node {\n"
        "    var next: Node?;\n"
        "    var val: int;\n"
        "    func getNext(): Node? {\n"
        "        return this.next;\n"
        "    }\n"
        "}\n"
        "func main(): int {\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_NO_THROW(parser.parse(tokens));
}

// ==========================================
// 2. コンパイル時エラー（Non-nullable安全検証）
// ==========================================

TEST(NullSafetyTest, NonNullableUninitializedThrows) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Node { var val: int; }\n"
        "func main(): int {\n"
        "    var n: Node;\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(NullSafetyTest, NonNullableAssignNullThrows) {
    clear_global_defs();
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Node { var val: int; }\n"
        "func main(): int {\n"
        "    var n: Node = null;\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(NullSafetyTest, NonNullableReassignNullThrows) {
    clear_global_defs();
    EXPECT_THROW(
        llvm_emit_ir(
            "class Node {\n"
            "    var val: int;\n"
            "    func Node(v: int) { this.val = v; }\n"
            "}\n"
            "func main(): int {\n"
            "    var n: Node = Node(10);\n"
            "    n = null;\n"
            "    return 0;\n"
            "}\n"
        ),
        CompileError
    );
}

// ==========================================
// 3. 実行レベル（正常系）
// ==========================================

TEST(NullSafetyTest, ExecNullableVarInitNull) {
    int res = run_llvm(
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "    func Point(x: int, y: int) { this.x = x; this.y = y; }\n"
        "}\n"
        "func main(): int {\n"
        "    var p: Point? = null;\n"
        "    if (p == null) {\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_EQ(res, 1);
}

TEST(NullSafetyTest, ExecNullableVarUninitializedIsDefaultNull) {
    int res = run_llvm(
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "}\n"
        "func main(): int {\n"
        "    var p: Point?;\n"
        "    if (p == null) {\n"
        "        return 42;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_EQ(res, 42);
}

TEST(NullSafetyTest, ExecNullableReassignInstance) {
    int res = run_llvm(
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "    func Point(x: int, y: int) { this.x = x; this.y = y; }\n"
        "}\n"
        "func main(): int {\n"
        "    var p: Point? = null;\n"
        "    p = Point(10, 20);\n"
        "    if (p != null) {\n"
        "        return p.x + p.y;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_EQ(res, 30);
}

TEST(NullSafetyTest, ExecNullableReassignNullAndInstance) {
    int res = run_llvm(
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "    func Point(x: int, y: int) { this.x = x; this.y = y; }\n"
        "}\n"
        "func main(): int {\n"
        "    var p: Point? = Point(10, 20);\n"
        "    p = null;\n"
        "    if (p == null) {\n"
        "        p = Point(30, 40);\n"
        "        return p.x + p.y;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_EQ(res, 70);
}

TEST(NullSafetyTest, ExecNullableFunctionParamAndReturn) {
    int res = run_llvm(
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "    func Point(x: int, y: int) { this.x = x; this.y = y; }\n"
        "}\n"
        "func makePoint(flag: int): Point? {\n"
        "    if (flag == 1) {\n"
        "        return Point(15, 25);\n"
        "    }\n"
        "    return null;\n"
        "}\n"
        "func getX(p: Point?): int {\n"
        "    if (p == null) {\n"
        "        return 0;\n"
        "    }\n"
        "    return p.x;\n"
        "}\n"
        "func main(): int {\n"
        "    var p1: Point? = makePoint(1);\n"
        "    var p2: Point? = makePoint(0);\n"
        "    var r1 = getX(p1);\n"
        "    var r2 = getX(p2);\n"
        "    return r1 + r2;\n"
        "}\n"
    );
    EXPECT_EQ(res, 15);
}

TEST(NullSafetyTest, ExecNullableLinkedListTraversal) {
    int res = run_llvm(
        "class Node {\n"
        "    var val: int;\n"
        "    var next: Node?;\n"
        "    func Node(v: int) {\n"
        "        this.val = v;\n"
        "        this.next = null;\n"
        "    }\n"
        "}\n"
        "func main(): int {\n"
        "    var n1 = Node(10);\n"
        "    var n2 = Node(20);\n"
        "    var n3 = Node(30);\n"
        "    n1.next = n2;\n"
        "    n2.next = n3;\n"
        "\n"
        "    var sum = 0;\n"
        "    var cur: Node? = n1;\n"
        "    while (cur != null) {\n"
        "        sum = sum + cur.val;\n"
        "        cur = cur.next;\n"
        "    }\n"
        "    return sum;\n"
        "}\n"
    );
    EXPECT_EQ(res, 60);
}

// ==========================================
// 4. 実行時 NullPointerException 検知
// ==========================================

TEST(NullSafetyTest, ExecNullPointerMemberAccessThrows) {
    int res = run_llvm(
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "}\n"
        "func main(): int {\n"
        "    var p: Point? = null;\n"
        "    var val = p.x;\n"
        "    return val;\n"
        "}\n"
    );
    // NullPointerException 発生時は exit(1) するため、終了コード 1 が返る
    EXPECT_EQ(res, 1);
}

TEST(NullSafetyTest, ExecNullPointerMethodCallThrows) {
    int res = run_llvm(
        "class Calculator {\n"
        "    var total: int;\n"
        "    func add(a: int): int {\n"
        "        return this.total + a;\n"
        "    }\n"
        "}\n"
        "func main(): int {\n"
        "    var c: Calculator? = null;\n"
        "    return c.add(10);\n"
        "}\n"
    );
    EXPECT_EQ(res, 1);
}
