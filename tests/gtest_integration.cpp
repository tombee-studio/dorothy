/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../include/cpu.hpp"
#include "../include/lexer.hpp"
#include "../include/parser.hpp"

static int run_program(const std::string& source) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(source);
    auto program = parser.parse(tokens);

    std::map<std::string, int> vars;
    std::map<std::string, int> functions;
    std::vector<Code> codes;
    for (auto func : program) {
        func->compile(codes, vars, functions, 4);
    }
    codes.insert(codes.begin(), Code::makeCode(Code::CALL, functions["main"], 0));
    codes.insert(codes.begin() + 1, Code::makeCode(Code::PUSHR, 2, 0));
    codes.insert(codes.begin() + 2, Code::makeCode(Code::POP, 2, 0));
    codes.insert(codes.begin() + 3, Code::makeCode(Code::EXIT, 0, 0));

    CPU cpu;
    cpu.set(codes);
    return cpu.exe();
}

// --- 算術演算 ---

TEST(IntegrationTest, ReturnConstant) {
    EXPECT_EQ(run_program("func main() { return 42; }"), 42);
}

TEST(IntegrationTest, Addition) {
    // test001 相当
    EXPECT_EQ(run_program("func main() { return 1 + 2; }"), 3);
}

TEST(IntegrationTest, Subtraction) {
    EXPECT_EQ(run_program("func main() { return 10 - 3; }"), 7);
}

TEST(IntegrationTest, Multiplication) {
    EXPECT_EQ(run_program("func main() { return 3 * 4; }"), 12);
}

TEST(IntegrationTest, Division) {
    EXPECT_EQ(run_program("func main() { return 10 / 2; }"), 5);
}

TEST(IntegrationTest, Modulo) {
    EXPECT_EQ(run_program("func main() { return 10 % 3; }"), 1);
}

// --- 比較演算 ---

TEST(IntegrationTest, CompareEQ) {
    EXPECT_EQ(run_program("func main() { return 3 == 3; }"), 1);
}

TEST(IntegrationTest, CompareEQ_False) {
    EXPECT_EQ(run_program("func main() { return 3 == 4; }"), 0);
}

TEST(IntegrationTest, CompareNE) {
    EXPECT_EQ(run_program("func main() { return 3 != 4; }"), 1);
}

TEST(IntegrationTest, CompareLT) {
    EXPECT_EQ(run_program("func main() { return 2 < 3; }"), 1);
}

TEST(IntegrationTest, CompareLE) {
    EXPECT_EQ(run_program("func main() { return 3 <= 3; }"), 1);
}

TEST(IntegrationTest, CompareGT) {
    EXPECT_EQ(run_program("func main() { return 4 > 3; }"), 1);
}

TEST(IntegrationTest, CompareGE) {
    EXPECT_EQ(run_program("func main() { return 3 >= 4; }"), 0);
}

// --- 変数 ---

TEST(IntegrationTest, VariableAssignment) {
    EXPECT_EQ(run_program(
        "func main() { var a: int = 5; return a; }"),
        5);
}

TEST(IntegrationTest, MultipleVariables) {
    EXPECT_EQ(run_program(
        "func main() { var a: int = 3; var b: int = 4; return a + b; }"),
        7);
}

// --- 制御フロー ---

TEST(IntegrationTest, IfTrueBranch) {
    EXPECT_EQ(run_program(
        "func main() { if(1 == 1) { return 1; } else { return 0; } }"),
        1);
}

TEST(IntegrationTest, IfFalseBranch) {
    EXPECT_EQ(run_program(
        "func main() { if(1 != 1) { return 1; } else { return 0; } }"),
        0);
}

TEST(IntegrationTest, WhileLoop) {
    EXPECT_EQ(run_program(
        "func main() {"
        "  var i: int = 0;"
        "  while(i < 5) { i = i + 1; }"
        "  return i;"
        "}"),
        5);
}

TEST(IntegrationTest, ForLoop) {
    // test003 相当: array[i] = i を埋めて array[3] を返す
    EXPECT_EQ(run_program(
        "func main() {"
        "  var array: int[5]; var i: int;"
        "  for(i = 0; i < 5; i = i + 1) { array[i] = i; }"
        "  return array[3];"
        "}"),
        3);
}

// --- 関数呼び出し ---

TEST(IntegrationTest, FunctionCall) {
    EXPECT_EQ(run_program(
        "func add(a: int, b: int) { return a + b; }"
        "func main() { return add(3, 4); }"),
        7);
}

TEST(IntegrationTest, FunctionCallPassByValue) {
    EXPECT_EQ(run_program(
        "func dbl(a: int) { return a + a; }"
        "func main() { return dbl(6); }"),
        12);
}

TEST(IntegrationTest, Recursion) {
    // test002 相当: fibonacci(10) = 10+9+...+2+1 = 55
    EXPECT_EQ(run_program(
        "func fibonacci(a: int) {"
        "  if(a > 1) { return a + fibonacci(a - 1); }"
        "  else { return 1; }"
        "}"
        "func main() { return fibonacci(10); }"),
        55);
}

// --- 配列 ---

TEST(IntegrationTest, ArrayReadWrite) {
    EXPECT_EQ(run_program(
        "func main() { var a: int[3]; a[0] = 10; a[1] = 20; a[2] = 30; return a[1]; }"),
        20);
}

// --- 変数型 ---

// float のビット表現を int として取得するヘルパー
static int float_bits(float f) {
    int bits;
    memcpy(&bits, &f, sizeof(float));
    return bits;
}

TEST(IntegrationTest, CharVariableAssignment) {
    // char は 1 バイト整数。値の代入と参照が正しく動作する
    EXPECT_EQ(run_program(
        "func main() { var c: char = 65; return c; }"),
        65);
}

TEST(IntegrationTest, LongVariableAssignment) {
    // long は 8 バイト整数。VM では int 幅で扱われるが代入・返却は正常
    EXPECT_EQ(run_program(
        "func main() { var l: long = 999; return l; }"),
        999);
}

TEST(IntegrationTest, FloatZeroValue) {
    // 0.0 のビット表現は 0 なので、float 変数に 0.0 を代入して返すと 0 になる
    EXPECT_EQ(run_program(
        "func main() { var x: float = 0.0; return x; }"),
        float_bits(0.0f));
}

TEST(IntegrationTest, FloatAddition) {
    // 1.0 + 1.0 = 2.0 のビット表現と一致することを確認
    EXPECT_EQ(run_program(
        "func main() { var x: float = 1.0; var y: float = 1.0; return x + y; }"),
        float_bits(2.0f));
}

TEST(IntegrationTest, FloatSubtraction) {
    EXPECT_EQ(run_program(
        "func main() { var x: float = 5.0; var y: float = 2.0; return x - y; }"),
        float_bits(3.0f));
}

TEST(IntegrationTest, FloatMultiplication) {
    EXPECT_EQ(run_program(
        "func main() { var x: float = 2.0; var y: float = 3.0; return x * y; }"),
        float_bits(6.0f));
}

TEST(IntegrationTest, FloatDivision) {
    EXPECT_EQ(run_program(
        "func main() { var x: float = 9.0; var y: float = 3.0; return x / y; }"),
        float_bits(3.0f));
}

TEST(IntegrationTest, CharFunctionArg) {
    // char 型引数に整数値を渡して返却
    EXPECT_EQ(run_program(
        "func identity(c: char) { return c; }"
        "func main() { return identity(65); }"),
        65);
}

TEST(IntegrationTest, LongFunctionArg) {
    // long 型引数に整数値を渡して返却
    EXPECT_EQ(run_program(
        "func identity(l: long) { return l; }"
        "func main() { return identity(999); }"),
        999);
}

TEST(IntegrationTest, VarDeclarationWithInit) {
    EXPECT_EQ(run_program(
        "func main() { var x: int = 21; return x + x; }"),
        42);
}

TEST(IntegrationTest, VarDeclarationNoInit) {
    // var without initializer defaults to 0
    EXPECT_EQ(run_program(
        "func main() { var x: int; return x; }"),
        0);
}

TEST(IntegrationTest, LetDeclaration) {
    EXPECT_EQ(run_program(
        "func main() { let n: int = 7; return n * 6; }"),
        42);
}

TEST(IntegrationTest, LetConstAssignThrows) {
    // let への再代入はコンパイルエラーになる
    EXPECT_THROW(run_program(
        "func main() { let x: int = 1; x = 2; return x; }"),
        CompileError);
}

TEST(IntegrationTest, VarFloatWithInit) {
    EXPECT_EQ(run_program(
        "func main() { var f: float = 2.0; return f * f; }"),
        float_bits(4.0f));
}

TEST(IntegrationTest, MultipleTypedVariables) {
    // 複数の型を同一スコープで使用
    EXPECT_EQ(run_program(
        "func main() {"
        "  var a: char = 1;"
        "  var b: int  = 2;"
        "  var c: long = 3;"
        "  return a + b + c;"
        "}"),
        6);
}
