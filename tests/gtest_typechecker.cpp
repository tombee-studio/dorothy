/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include "../include/typechecker.hpp"

static std::vector<Function *> parse(const std::string &src) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(src);
    return parser.parse(tokens);
}

// --- 正常系: 型が合っている場合はエラーにならない ---

TEST(TypeCheckerTest, IntReturnInt_OK) {
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse("func f(): int { return 1; }")));
}

TEST(TypeCheckerTest, LongReturnLong_OK) {
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse("func f(): long { return 1; }")));
}

TEST(TypeCheckerTest, DoubleReturnDouble_OK) {
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse("func f(): double { return 3.14; }")));
}

TEST(TypeCheckerTest, FloatReturnFloat_OK) {
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse("func f(): float { return 1.0; }")));
}

TEST(TypeCheckerTest, IntReturnIntArith_OK) {
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse("func f(a: int, b: int): int { return a + b; }")));
}

TEST(TypeCheckerTest, DoubleReturnDoubleArith_OK) {
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse("func f(): double { return 1.0 + 2.0; }")));
}

TEST(TypeCheckerTest, DoubleReturnIntImplicitConversion_OK) {
    // int を double 関数から返すのは暗黙変換として許容する
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse("func f(): double { return 1; }")));
}

TEST(TypeCheckerTest, NoReturnTypeAnnotation_OK) {
    // 戻り値型アノテーションなし → 型チェックをスキップ
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse("func f() { return 3.14; }")));
}

TEST(TypeCheckerTest, FunctionCallReturnType_OK) {
    // 呼び出し先の戻り値型が一致する場合
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse(
        "func g(): int { return 1; } "
        "func f(): int { return g(); }")));
}

TEST(TypeCheckerTest, ReturnInIfElse_OK) {
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse(
        "func f(a: int): int { "
        "  if (a > 0) { return 1; } "
        "  else { return 0; } "
        "  return 0; "
        "}")));
}

TEST(TypeCheckerTest, ReturnInWhile_OK) {
    TypeChecker tc;
    EXPECT_NO_THROW(tc.check(parse(
        "func f(n: int): int { "
        "  while (n > 0) { return n; } "
        "  return 0; "
        "}")));
}

// --- 異常系: 型が合わない場合はエラーになる ---

TEST(TypeCheckerTest, IntReturnFloat_Error) {
    TypeChecker tc;
    EXPECT_THROW(tc.check(parse("func f(): int { return 3.14; }")), TypeCheckError);
}

TEST(TypeCheckerTest, LongReturnFloat_Error) {
    TypeChecker tc;
    EXPECT_THROW(tc.check(parse("func f(): long { return 1.0; }")), TypeCheckError);
}

TEST(TypeCheckerTest, CharReturnDouble_Error) {
    TypeChecker tc;
    EXPECT_THROW(tc.check(parse("func f(): char { return 3.14; }")), TypeCheckError);
}

TEST(TypeCheckerTest, IntReturnFloatArith_Error) {
    // 浮動小数演算の結果を int 関数から返す
    TypeChecker tc;
    EXPECT_THROW(tc.check(parse("func f(): int { return 1.0 + 2.0; }")), TypeCheckError);
}

TEST(TypeCheckerTest, IntReturnMixedArith_Error) {
    // int + float → double として扱われる → int 関数から返せない
    TypeChecker tc;
    EXPECT_THROW(tc.check(parse("func f(a: int): int { return a + 1.0; }")), TypeCheckError);
}

TEST(TypeCheckerTest, IntReturnFloatFuncCall_Error) {
    // double を返す関数を int 関数から返す
    TypeChecker tc;
    EXPECT_THROW(tc.check(parse(
        "func g(): double { return 3.14; } "
        "func f(): int { return g(); }")), TypeCheckError);
}

TEST(TypeCheckerTest, IntReturnFloatInIf_Error) {
    // if ブランチ内で float を return
    TypeChecker tc;
    EXPECT_THROW(tc.check(parse(
        "func f(a: int): int { "
        "  if (a > 0) { return 1.5; } "
        "  return 0; "
        "}")), TypeCheckError);
}

TEST(TypeCheckerTest, IntReturnFloatVarInBranch_Error) {
    // double 型変数を int 関数から返す
    TypeChecker tc;
    EXPECT_THROW(tc.check(parse(
        "func f(): int { "
        "  var x: double = 3.14; "
        "  return x; "
        "}")), TypeCheckError);
}
