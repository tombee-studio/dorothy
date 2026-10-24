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
            "    constructor(v: int) { this.val = v; }\n"
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
        "    constructor(x: int, y: int) { this.x = x; this.y = y; }\n"
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
        "    constructor(x: int, y: int) { this.x = x; this.y = y; }\n"
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
        "    constructor(x: int, y: int) { this.x = x; this.y = y; }\n"
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
        "    constructor(x: int, y: int) { this.x = x; this.y = y; }\n"
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
        "    constructor(v: int) {\n"
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
// 4. Null参照をコンパイル時に拒否
// ==========================================

TEST(NullSafetyTest, CompileRejectsNullPointerMemberAccess) {
    EXPECT_THROW(llvm_emit_ir(
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "}\n"
        "func main(): int {\n"
        "    var p: Point? = null;\n"
        "    var val = p.x;\n"
        "    return val;\n"
        "}\n"
    ), CompileError);
}

TEST(NullSafetyTest, CompileRejectsNullPointerMethodCall) {
    EXPECT_THROW(llvm_emit_ir(
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
    ), CompileError);
}

static void check_source(const std::string &source) {
    Lexer lexer; Parser parser;
    auto tokens = lexer.lex(source);
    parser.parse(tokens);
}

static const std::string null_node =
    "class Node { var x: int; var next: Node?; "
    "func value() -> int { return this.x; } } ";

TEST(StaticNullSafetyTest, RejectsNestedUnsafeExpressions) {
    for (const auto &code : {
        "func main() -> int { var n: Node? = null; return 1 + n.x; }",
        "func main() -> int { var n: Node? = null; if(n.value() > 0) { return 1; } return 0; }",
        "func main() -> int { var n: Node? = null; var a: int[1] = {n.x}; return 0; }",
        "func main() -> int { var a: Array<int>? = null; return a[0]; }",
        "func main() -> int { var a: Array<int>? = null; a.push(1); return 0; }",
        "func main() -> int { var n = Node(); return n.next.x; }"})
        EXPECT_THROW(check_source(null_node + code), CompileError) << code;
}

TEST(StaticNullSafetyTest, AcceptsGuardsAndEarlyReturns) {
    for (const auto &code : {
        "func f(n: Node?) -> int { if(n != null) { return n.value(); } return 0; }",
        "func f(n: Node?) -> int { if(null == n) { return 0; } return n.x; }",
        "func f(n: Node?) -> int { if(n == null) { n = Node(); } return n.x; }",
        "func f(n: Node?) -> int { if(n != null) { var x = n.x; return x; } else { return 0; } }",
        "func f() -> int { var n: Node? = Node(); return n.x; }",
        "func f(n: Node?) -> int { var sum = 0; while(n != null) { sum = sum + n.x; n = n.next; } return sum; }"})
        EXPECT_NO_THROW(check_source(null_node + code)) << code;
}

TEST(StaticNullSafetyTest, JoinsBranchesAndInvalidatesAssignments) {
    for (const auto &code : {
        "func f(n: Node?, flag: int) -> int { if(flag) { n = Node(); } return n.x; }",
        "func f(n: Node?) -> int { if(n != null) { n = null; return n.x; } return 0; }",
        "func f(n: Node?, flag: int) -> int { if(n != null) { if(flag) { n = null; } return n.x; } return 0; }",
        "func f() -> int { var n: Node? = Node(); { n = null; var n = Node(); } return n.x; }"})
        EXPECT_THROW(check_source(null_node + code), CompileError) << code;
    EXPECT_NO_THROW(check_source(null_node +
        "func f() -> int { var n: Node? = null; { var n = Node(); n.x = 3; } if(n == null) { return 1; } return n.x; }"));
}

TEST(StaticNullSafetyTest, LoopBackEdgesCannotKeepStaleProofs) {
    EXPECT_THROW(check_source(null_node +
        "func f(flag: int) -> int { var n: Node? = Node(); while(flag) { var x = n.x; n = null; } return 0; }"), CompileError);
    EXPECT_THROW(check_source(null_node +
        "func f(flag: int) -> int { var n: Node? = Node(); for(flag = 1; flag; flag = flag - 1) { var x = n.x; n = null; } return 0; }"), CompileError);
    EXPECT_THROW(check_source(null_node +
        "func f(n: Node?) -> int { while(n != null) { n = n.next; } return n.x; }"), CompileError);
}

TEST(StaticNullSafetyTest, ChecksArgumentsAndReturnContracts) {
    for (const auto &code : {
        "func need(n: Node) -> int { return n.x; } func f(n: Node?) -> int { return need(n); }",
        "func f(n: Node?) -> Node { return n; }",
        "func f() -> Node { return null; }",
        "func f(flag: int) -> Node { if(flag) { return Node(); } }",
        "class Holder { constructor(n: Node) {} } func f(n: Node?) -> int { var h = Holder(n); return 0; }",
        "class Holder { func set(n: Node) -> int { return n.x; } } func f(n: Node?) -> int { var h = Holder(); return h.set(n); }",
        "func maybe() -> Node? { return null; } func f() -> int { var n = maybe(); return n.x; }",
        "func f(n: Node?) -> int { var sure: Node = n; return 0; }",
        "func f() -> int { var n: Node? = 0; return 0; }"})
        EXPECT_THROW(check_source(null_node + code), CompileError) << code;
    EXPECT_NO_THROW(check_source(null_node +
        "func need(n: Node) -> Node { return n; } func f(n: Node?) -> Node { if(n == null) { return Node(); } return need(n); }"));
}

TEST(StaticNullSafetyTest, FieldsAndAliasingRequireFreshProofs) {
    const std::string box = null_node + "class Box { var node: Node?; func clear() { this.node = null; } } ";
    EXPECT_THROW(check_source(box +
        "func f(b: Box) -> int { if(b.node != null) { b.clear(); return b.node.x; } return 0; }"), CompileError);
    EXPECT_THROW(check_source(box +
        "func f(b: Box) -> int { var alias = b; if(b.node != null) { alias.node = null; return b.node.x; } return 0; }"), CompileError);
    EXPECT_NO_THROW(check_source(box +
        "func f(b: Box) -> int { var n = b.node; if(n != null) { b.clear(); return n.x; } return 0; }"));
}

TEST(StaticNullSafetyTest, ChecksConstructorsAndThisEscape) {
    for (const auto &code : {
        "class Box { var node: Node; }",
        "class Box { var node: Node; constructor(flag: int) { if(flag) { this.node = Node(); } } }",
        "class Box { var node: Node; constructor() { this.node.x = 1; this.node = Node(); } }",
        "class Box { var node: Node; constructor() { var self = this; this.node = Node(); } }",
        "class Box { var node: Node; constructor() { this.fill(); } func fill() { this.node = Node(); } }"})
        EXPECT_THROW(check_source(null_node + code), CompileError) << code;
    EXPECT_NO_THROW(check_source(null_node +
        "class Box { var node: Node; constructor(n: Node) { this.node = n; } } "
        "class Child: Box { constructor(n: Node) { super(n); } }"));
}

TEST(StaticNullSafetyTest, ChecksArrayElementsAndMutableVariance) {
    for (const auto &code : {
        "func f(n: Node?) -> int { var a: Array<Node> = [n]; return 0; }",
        "func f(n: Node?) -> int { var a: Array<Node> = []; a.push(n); return 0; }",
        "func f(n: Node?) -> int { var a: Array<Node> = []; a[0] = n; return 0; }",
        "func f() -> int { var a: Array<Node?> = [null]; return a[0].x; }",
        "func f() -> int { var a: Array<Node> = []; var b: Array<Node?> = a; return 0; }"})
        EXPECT_THROW(check_source(null_node + code), CompileError) << code;
    EXPECT_NO_THROW(check_source(null_node +
        "func f() -> int { var a: Array<Node?> = [null]; var n = a[0]; if(n == null) { return 0; } return n.x; }"));
}

TEST(StaticNullSafetyTest, OverridesPreserveNullContracts) {
    EXPECT_THROW(check_source(null_node +
        "class A { func get() -> Node { return Node(); } } "
        "class B: A { override func get() -> Node? { return null; } }"), CompileError);
    EXPECT_THROW(check_source(null_node +
        "class A { func put(n: Node?) {} } class B: A { override func put(n: Node) {} }"), CompileError);
}

TEST(StaticNullSafetyTest, RejectsVirtualDispatchDuringConstruction) {
    EXPECT_THROW(check_source(null_node +
        "class A { constructor() { this.ready(); } func ready() {} } "
        "class B: A { var node: Node; constructor() { super(); this.node = Node(); } "
        "override func ready() { this.node.x = 1; } }"), CompileError);
}

TEST(StaticNullSafetyTest, NestedStructReferencesMustBeInitialized) {
    EXPECT_THROW(check_source(null_node +
        "struct Inner { var node: Node; constructor(n: Node) { this.node = n; } } "
        "struct Outer { var inner: Inner; }"), CompileError);
    EXPECT_THROW(check_source(null_node +
        "struct Inner { var node: Node; constructor(n: Node) { this.node = n; } } "
        "struct Outer { var inner: Inner; constructor() { var copy = this.inner; this.inner.node = Node(); } }"), CompileError);
    EXPECT_NO_THROW(check_source(null_node +
        "struct Inner { var node: Node; constructor(n: Node) { this.node = n; } } "
        "struct Outer { var inner: Inner; constructor(n: Node) { this.inner.node = n; } }"));
}

TEST(StaticNullSafetyTest, RejectsMissingReferenceArguments) {
    EXPECT_THROW(check_source(null_node +
        "func need(n: Node) -> int { return n.x; } func main() -> int { return need(); }"), CompileError);
    EXPECT_THROW(check_source(null_node +
        "class Box { constructor(n: Node) {} } func main() -> int { var b = Box(); return 0; }"), CompileError);
    EXPECT_THROW(check_source(null_node +
        "class Box { func need(n: Node) -> int { return n.x; } } func main() -> int { var b = Box(); return b.need(); }"), CompileError);
}

TEST(StaticNullSafetyTest, EmptyArrayAliasCannotEraseElementContracts) {
    EXPECT_THROW(check_source(null_node +
        "func main() -> int { var a = []; var b: Array<Node> = a; return 0; }"), CompileError);
}

TEST(StaticNullSafetyTest, NativeInferredNullableReturnAndAlias) {
    EXPECT_EQ(run_llvm(
        "class Node { var x: int; constructor(x: int) { this.x = x; } } "
        "func maybe(flag: int) -> Node? { if(flag) { return Node(42); } return null; } "
        "func main() -> int { var n = maybe(1); if(n == null) { return 0; } "
        "var sure: Node = n; n = null; return sure.x; }"), 42);
}

TEST(StaticNullSafetyTest, NativeNullableStringNullIsNotEmptyString) {
    EXPECT_EQ(run_llvm(
        "func main() -> int { var s: string? = \"\"; if(s == null) { return 1; } "
        "s = null; if(s != null) { return 2; } return 42; }"), 42);
}

TEST(StaticNullSafetyTest, NativeNullableArrayGuard) {
    EXPECT_EQ(run_llvm(
        "func length(a: Array<int>?) -> int { if(a == null) { return 0; } return a.length; } "
        "func main() -> int { var a: Array<int>? = null; a = [7, 8]; return length(a); }"), 2);
}

TEST(StaticNullSafetyTest, NativeNullableConstructorParameterAndMethodReturn) {
    EXPECT_EQ(run_llvm(
        "class Node { var x: int; constructor(x: int) { this.x = x; } func value() -> int { return this.x; } } "
        "class Box { var node: Node?; var copied: int; constructor(n: Node?) { this.node = n; "
        "if(n != null) { this.copied = n.value(); } } func get() -> Node? { return this.node; } } "
        "func main() -> int { var b = Box(Node(21)); var n = b.get(); "
        "if(n == null) { return 0; } return n.x + b.copied; }"), 42);
}

TEST(StaticNullSafetyTest, NonNullFieldsAndSuperArgumentsRejectNullableValues) {
    EXPECT_THROW(check_source(null_node +
        "class Box { var node: Node; constructor(n: Node) { this.node = n; } } "
        "func f(b: Box, n: Node?) { b.node = n; }"), CompileError);
    EXPECT_THROW(check_source(null_node +
        "class Box { constructor(n: Node) {} } class Child: Box { constructor(n: Node?) { super(n); } }"), CompileError);
}
