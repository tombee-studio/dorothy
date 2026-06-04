/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include "../include/lexer.hpp"

TEST(LexerTest, EmptyInputEndsWithEOF) {
    Lexer lexer;
    auto tokens = lexer.lex("");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, Token::TK_EOF);
}

TEST(LexerTest, LexInteger) {
    Lexer lexer;
    auto tokens = lexer.lex("42");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_INT);
    EXPECT_EQ(tokens[0].int_val, 42);
    EXPECT_EQ(tokens[1].type, Token::TK_EOF);
}

TEST(LexerTest, LexZero) {
    Lexer lexer;
    auto tokens = lexer.lex("0");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_INT);
    EXPECT_EQ(tokens[0].int_val, 0);
}

TEST(LexerTest, LexIdentifier) {
    Lexer lexer;
    auto tokens = lexer.lex("foo");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_ID);
    EXPECT_EQ(tokens[0].id, "foo");
}

TEST(LexerTest, LexIdentifierWithUnderscore) {
    Lexer lexer;
    auto tokens = lexer.lex("foo_bar");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_ID);
    EXPECT_EQ(tokens[0].id, "foo_bar");
}

TEST(LexerTest, LexKeywordInt) {
    Lexer lexer;
    auto tokens = lexer.lex("int");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::KW_INT);
}

TEST(LexerTest, LexKeywordFunc) {
    Lexer lexer;
    auto tokens = lexer.lex("func");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::KW_FUNC);
}

TEST(LexerTest, LexKeywordIf) {
    Lexer lexer;
    auto tokens = lexer.lex("if");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::KW_IF);
}

TEST(LexerTest, LexKeywordElse) {
    Lexer lexer;
    auto tokens = lexer.lex("else");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::KW_ELSE);
}

TEST(LexerTest, LexKeywordWhile) {
    Lexer lexer;
    auto tokens = lexer.lex("while");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::KW_WHILE);
}

TEST(LexerTest, LexKeywordReturn) {
    Lexer lexer;
    auto tokens = lexer.lex("return");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::KW_RETURN);
}

TEST(LexerTest, LexKeywordFor) {
    Lexer lexer;
    auto tokens = lexer.lex("for");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::KW_FOR);
}

TEST(LexerTest, KeywordNotMatchedInsideIdentifier) {
    Lexer lexer;
    auto tokens = lexer.lex("ifoo");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_ID);
    EXPECT_EQ(tokens[0].id, "ifoo");
}

TEST(LexerTest, LexOperatorPlus) {
    Lexer lexer;
    auto tokens = lexer.lex("+");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, static_cast<Token::Type>('+'));
}

TEST(LexerTest, LexOperatorMinus) {
    Lexer lexer;
    auto tokens = lexer.lex("-");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, static_cast<Token::Type>('-'));
}

TEST(LexerTest, LexOperatorStar) {
    Lexer lexer;
    auto tokens = lexer.lex("*");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, static_cast<Token::Type>('*'));
}

TEST(LexerTest, LexOperatorSlash) {
    Lexer lexer;
    auto tokens = lexer.lex("/");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, static_cast<Token::Type>('/'));
}

TEST(LexerTest, LexOperatorAssign) {
    Lexer lexer;
    auto tokens = lexer.lex("=");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, static_cast<Token::Type>('='));
}

TEST(LexerTest, LexOperatorEQ) {
    Lexer lexer;
    auto tokens = lexer.lex("==");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_EQ);
}

TEST(LexerTest, LexOperatorNE) {
    Lexer lexer;
    auto tokens = lexer.lex("!=");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_NE);
}

TEST(LexerTest, LexOperatorLE) {
    Lexer lexer;
    auto tokens = lexer.lex("<=");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_LE);
}

TEST(LexerTest, LexOperatorGE) {
    Lexer lexer;
    auto tokens = lexer.lex(">=");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_GE);
}

TEST(LexerTest, LexArithmeticExpression) {
    Lexer lexer;
    auto tokens = lexer.lex("1 + 2");
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].type, Token::TK_INT);
    EXPECT_EQ(tokens[0].int_val, 1);
    EXPECT_EQ(tokens[1].type, static_cast<Token::Type>('+'));
    EXPECT_EQ(tokens[2].type, Token::TK_INT);
    EXPECT_EQ(tokens[2].int_val, 2);
    EXPECT_EQ(tokens[3].type, Token::TK_EOF);
}

TEST(LexerTest, LexFunctionDeclaration) {
    Lexer lexer;
    // func main ( ) { return 1 ; } EOF => 9 tokens
    auto tokens = lexer.lex("func main() { return 1; }");
    ASSERT_GE(tokens.size(), 9u);
    EXPECT_EQ(tokens[0].type, Token::KW_FUNC);
    EXPECT_EQ(tokens[1].type, Token::TK_ID);
    EXPECT_EQ(tokens[1].id, "main");
    EXPECT_EQ(tokens.back().type, Token::TK_EOF);
}

TEST(LexerTest, LexCharLiteral) {
    Lexer lexer;
    auto tokens = lexer.lex("'A'");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, Token::TK_INT);
    EXPECT_EQ(tokens[0].int_val, 'A');
}

TEST(LexerTest, LexUndefinedTokenThrows) {
    Lexer lexer;
    EXPECT_THROW(lexer.lex("@"), LexerError);
}
