/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include "../include/lexer.hpp"
#include "../include/parser.hpp"

TEST(ParserTest, ParseSimpleFunction) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { return 1; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
    EXPECT_FALSE(program[0]->isImport());
}

TEST(ParserTest, ParseFunctionWithOneArg) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func dbl(a: int) { return a; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "dbl");
    EXPECT_FALSE(program[0]->isImport());
}

TEST(ParserTest, ParseFunctionWithMultipleArgs) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func add(a: int, b: int) { return a; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "add");
}

TEST(ParserTest, ParseMultipleFunctions) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func foo() { return 1; } "
        "func bar() { return 2; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 2u);
    EXPECT_EQ(program[0]->getName(), "foo");
    EXPECT_EQ(program[1]->getName(), "bar");
}

TEST(ParserTest, ParseFunctionCallInReturn) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func test(a: int) { return a; } "
        "func main() { return test(3); }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 2u);
    EXPECT_EQ(program[0]->getName(), "test");
    EXPECT_EQ(program[1]->getName(), "main");
}

TEST(ParserTest, ParseFibonacci) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func fibonacci(a: int) { "
        "  if(a > 1) { return a + fibonacci(a - 1); } "
        "  else { return 1; } "
        "} "
        "func main() { return fibonacci(10); }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 2u);
    EXPECT_EQ(program[0]->getName(), "fibonacci");
    EXPECT_EQ(program[1]->getName(), "main");
}

TEST(ParserTest, ParseForLoop) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func main() { "
        "  var i: int; "
        "  for(i = 0; i < 5; i = i + 1) { } "
        "  return i; "
        "}");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, ParseWhileLoop) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func main() { "
        "  var i: int = 0; "
        "  while(i < 10) { i = i + 1; } "
        "  return i; "
        "}");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, ParseImportFunction) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "import print; "
        "func main() { return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 2u);
    EXPECT_TRUE(program[0]->isImport());
    EXPECT_EQ(program[0]->getName(), "print");
    EXPECT_FALSE(program[1]->isImport());
    EXPECT_EQ(program[1]->getName(), "main");
}

// --- 変数型宣言 ---

TEST(ParserTest, ParseCharVariableDecl) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var c: char; return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, ParseLongVariableDecl) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var l: long; return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, ParseFloatVariableDecl) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var f: float; return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, ParseDoubleVariableDecl) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var d: double; return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, ParseFloatLiteralAssignment) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var x: float = 3.14; return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, OldStyleDeclRejected) {
    // 旧来の C スタイル変数宣言はパースエラーになる
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { int x; return x; }");
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ParserTest, ParseVarDecl) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var x: int = 0; return x; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, ParseLetDecl) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { let x: double = 0.0; return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "main");
}

TEST(ParserTest, ParseVarNoInitializer) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { var x: int; return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
}

TEST(ParserTest, ParseLetRequiresInitializer) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { let x: int; return 0; }");
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ParserTest, ParseTypedFunctionArgs) {
    Lexer lexer;
    Parser parser;
    // 全型を引数に持つ関数がパースできること
    auto tokens = lexer.lex(
        "func f(a: char, b: int, c: long) { return 0; } "
        "func g(x: float, y: double) { return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 2u);
    EXPECT_EQ(program[0]->getName(), "f");
    EXPECT_EQ(program[1]->getName(), "g");
}

// --- 戻り値型アノテーション ---

TEST(ParserTest, ParseFunctionWithIntReturnType) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func add(a: int, b: int): int { return a; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "add");
    EXPECT_EQ(program[0]->getRetType(), VarType::INT);
    EXPECT_TRUE(program[0]->hasExplicitRetType());
}

TEST(ParserTest, ParseFunctionWithLongReturnType) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func f(): long { return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getRetType(), VarType::LONG);
    EXPECT_TRUE(program[0]->hasExplicitRetType());
}

TEST(ParserTest, ParseFunctionWithDoubleReturnType) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func pi(): double { return 3; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getRetType(), VarType::DOUBLE);
    EXPECT_TRUE(program[0]->hasExplicitRetType());
}

TEST(ParserTest, ParseFunctionWithFloatReturnType) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func f(): float { return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getRetType(), VarType::FLOAT);
    EXPECT_TRUE(program[0]->hasExplicitRetType());
}

TEST(ParserTest, ParseFunctionWithCharReturnType) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func f(): char { return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getRetType(), VarType::CHAR);
    EXPECT_TRUE(program[0]->hasExplicitRetType());
}

TEST(ParserTest, ParseFunctionWithoutReturnTypeHasNoExplicitRetType) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func main() { return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_FALSE(program[0]->hasExplicitRetType());
}

TEST(ParserTest, ParseFunctionReturnTypeInvalidKeyword) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func f(): badtype { return 0; }");
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ParserTest, ParseFibonacciWithReturnType) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "func fib(n: int): int { "
        "  if (n == 1) { return 1; } "
        "  if (n == 2) { return 1; } "
        "  return fib(n - 1) + fib(n - 2); "
        "} "
        "func main(): int { return fib(10); }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 2u);
    EXPECT_EQ(program[0]->getName(), "fib");
    EXPECT_EQ(program[0]->getRetType(), VarType::INT);
    EXPECT_TRUE(program[0]->hasExplicitRetType());
    EXPECT_EQ(program[1]->getName(), "main");
    EXPECT_TRUE(program[1]->hasExplicitRetType());
}
