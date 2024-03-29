#include "../include/token.hpp"
#include <gtest/gtest.h>
using namespace Dorothy;

TEST(TokenExactlyCreateTest, TokenExactlyCreateIntegerTest1) {
    auto token = Token::createInteger(1);
    EXPECT_EQ(token.getInteger(), 1);
    EXPECT_EQ(token.getType(), Token::TK_INT);
}

TEST(TokenExactlyCreateTest, TokenExactlyCreateIntegerTest2) {
    auto token = Token::createInteger(-1);
    EXPECT_EQ(token.getInteger(), -1);
    EXPECT_EQ(token.getType(), Token::TK_INT);
}

TEST(TokenExactlyCreateTest, TokenExactlyCreateOperatorsTest1) {
    auto token = Token::createToken('+');
    EXPECT_EQ(token.getType(), Token::TK_ADD);
    EXPECT_EQ(token.getType(), '+');
}

TEST(TokenExactlyCreateTest, TokenExactlyCreateOperatorsTest2) {
    auto token = Token::createToken('-');
    EXPECT_EQ(token.getType(), Token::TK_SUB);
    EXPECT_EQ(token.getType(), '-');
}

TEST(TokenExactlyCreateTest, TokenExactlyCreateOperatorsTest3) {
    auto token = Token::createToken('*');
    EXPECT_EQ(token.getType(), Token::TK_MUL);
    EXPECT_EQ(token.getType(), '*');
}

TEST(TokenExactlyCreateTest, TokenExactlyCreateOperatorsTest4) {
    auto token = Token::createToken('/');
    EXPECT_EQ(token.getType(), Token::TK_DIV);
    EXPECT_EQ(token.getType(), '/');
}

TEST(TokenExactlyCreateTest, TokenExactlyCreateOperatorsTest5) {
    auto token = Token::createToken('%');
    EXPECT_EQ(token.getType(), Token::TK_MOD);
    EXPECT_EQ(token.getType(), '%');
}
