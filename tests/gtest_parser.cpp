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
    auto tokens = lexer.lex("func double(int a) { return a; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0]->getName(), "double");
    EXPECT_FALSE(program[0]->isImport());
}

TEST(ParserTest, ParseFunctionWithMultipleArgs) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func add(int a, int b) { return a; }");
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
        "func test(int a) { return a; } "
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
        "func fibonacci(int a) { "
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
        "  int i; "
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
        "  int i; "
        "  i = 0; "
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
