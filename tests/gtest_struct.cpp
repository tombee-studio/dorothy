/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "../include/ast.hpp"
#include "../include/lexer.hpp"
#include "../include/llvm_gen.hpp"
#include "../include/parser.hpp"

// LLVM IR を文字列として生成するヘルパー
static std::string llvm_emit_ir(const std::string& source) {
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

// ===== パーサーレベル（正常系） =====

// 構造体型パラメータをパースできる
TEST(StructTest, ParseStructParam) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "struct Point { var x: int; var y: int; } "
        "func foo(p: Point) { return 0; } "
        "func main() { return 0; }");
    EXPECT_NO_THROW(parser.parse(tokens));
}

// -> 戻り値型をパースできる
TEST(StructTest, ParseStructReturnType) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "struct Point { var x: int; } "
        "func getP() -> Point { var p: Point = Point(1); return p; } "
        "func main() { var r: Point = getP(); return r.x; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 2u);
    EXPECT_EQ(program[0]->getRetType(), VarType::STRUCT);
    EXPECT_EQ(program[0]->getRetStructName(), "Point");
}

// -> のない関数はデフォルト戻り値型
TEST(StructTest, ParseNonStructReturnIsDefault) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func foo() { return 0; }");
    auto program = parser.parse(tokens);
    ASSERT_EQ(program.size(), 1u);
    EXPECT_NE(program[0]->getRetType(), VarType::STRUCT);
    EXPECT_TRUE(program[0]->getRetStructName().empty());
}

// ===== パーサーレベル（異常系） =====

// 未定義の構造体を戻り値型に指定するとエラー
TEST(StructTest, UnknownReturnStructThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex("func foo() -> Unknown { return 0; }");
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

// コンストラクタ引数に構造体型はエラー
TEST(StructTest, ConstructorWithStructParamThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "struct P { var x: int; } "
        "struct Q { var y: int; constructor(p: P) {} }");
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

// ===== LLVM IR 生成（正常系） =====

// 構造体引数はフィールドごとに展開されて渡される
TEST(StructTest, StructParamExpandedToFields) {
    auto ir = llvm_emit_ir(
        "struct Point { var x: int; var y: int; } "
        "func getX(p: Point) { return p.x; } "
        "func main() { var a: Point = Point(1, 2); return getX(a); }");
    // Point{x: int, y: int} → i32 x 2
    EXPECT_NE(ir.find("define i64 @getX(i32 %param.0, i32 %param.1)"),
              std::string::npos);
}

// 構造体戻り値は void + sret ポインタパターン
TEST(StructTest, StructReturnUsesSretPattern) {
    auto ir = llvm_emit_ir(
        "struct Point { var x: int; var y: int; } "
        "func getP() -> Point { var p: Point = Point(3, 4); return p; } "
        "func main() { var r: Point = getP(); return r.x; }");
    EXPECT_NE(ir.find("define void @getP(ptr %sret.0, ptr %sret.1)"),
              std::string::npos);
}

// 構造体戻り値の呼び出しは void call になる
TEST(StructTest, StructReturnCallSiteEmitsVoidCall) {
    auto ir = llvm_emit_ir(
        "struct Point { var x: int; var y: int; } "
        "func getP() -> Point { var p: Point = Point(3, 4); return p; } "
        "func main() { var r: Point = getP(); return r.x; }");
    EXPECT_NE(ir.find("call void @getP("), std::string::npos);
}

// 呼び出し側で構造体フィールドがロードされて渡される（値渡し）
TEST(StructTest, StructArgCallPassesFieldValues) {
    auto ir = llvm_emit_ir(
        "struct Point { var x: int; var y: int; } "
        "func getX(p: Point) { return p.x; } "
        "func main() { var a: Point = Point(5, 6); return getX(a); }");
    // 呼び出しは i32 型の値で行われる（ポインタではなく値渡し）
    EXPECT_NE(ir.find("call i64 @getX(i32"), std::string::npos);
}

// 構造体引数 + 構造体戻り値の複合パターン
TEST(StructTest, StructParamAndReturn) {
    auto ir = llvm_emit_ir(
        "struct Point { var x: int; var y: int; } "
        "func scale(p: Point, f: int) -> Point {"
        "  var r: Point = Point(p.x * f, p.y * f);"
        "  return r;"
        "} "
        "func main() {"
        "  var a: Point = Point(2, 3);"
        "  var b: Point = scale(a, 10);"
        "  return b.x + b.y;"
        "}");
    // sret ptrs + Point展開(2 x i32) + int(i32)
    EXPECT_NE(
        ir.find("define void @scale(ptr %sret.0, ptr %sret.1, i32 %param.0, i32 %param.1, i32 %param.2)"),
        std::string::npos);
}

// 前方参照: main で getP() を呼ぶが getP は後で定義されている
TEST(StructTest, ForwardReferenceStructReturn) {
    auto ir = llvm_emit_ir(
        "struct Point { var x: int; var y: int; } "
        "func main() {"
        "  var r: Point = getP();"
        "  return r.x;"
        "} "
        "func getP() -> Point {"
        "  var p: Point = Point(5, 6);"
        "  return p;"
        "}");
    EXPECT_NE(ir.find("call void @getP("), std::string::npos);
}

// 値渡し: 呼び出し先でフィールドを変更しても呼び出し元に影響しない（IR レベル確認）
TEST(StructTest, StructPassByValueDoesNotPassPointers) {
    auto ir = llvm_emit_ir(
        "struct Pair { var a: int; var b: int; } "
        "func modify(p: Pair) { p.a = 99; return 0; } "
        "func main() {"
        "  var x: Pair = Pair(1, 2);"
        "  modify(x);"
        "  return x.a;"
        "}");
    // modify の呼び出しは i32 値渡し（ptr ではない）
    EXPECT_NE(ir.find("call i64 @modify(i32"), std::string::npos);
}

// 複数の構造体引数
TEST(StructTest, MultipleStructParams) {
    auto ir = llvm_emit_ir(
        "struct V2 { var x: int; var y: int; } "
        "func add(a: V2, b: V2) { return a.x + b.x; } "
        "func main() {"
        "  var p: V2 = V2(1, 2);"
        "  var q: V2 = V2(3, 4);"
        "  return add(p, q);"
        "}");
    // a(2 fields) + b(2 fields) = 4 i32 params
    EXPECT_NE(
        ir.find("define i64 @add(i32 %param.0, i32 %param.1, i32 %param.2, i32 %param.3)"),
        std::string::npos);
}

// ===== LLVM IR 生成（異常系） =====

// 構造体戻り値の関数を return 式に使うとエラー
TEST(StructTest, StructReturnFuncAsReturnExprThrows) {
    EXPECT_THROW(
        llvm_emit_ir(
            "struct Point { var x: int; } "
            "func getP() -> Point { var p: Point = Point(1); return p; } "
            "func main() { return getP(); }"),
        CompileError);
}

// 構造体戻り値の関数を文として使うとエラー
TEST(StructTest, StructReturnFuncAsStatementThrows) {
    EXPECT_THROW(
        llvm_emit_ir(
            "struct Point { var x: int; } "
            "func getP() -> Point { var p: Point = Point(1); return p; } "
            "func main() { getP(); return 0; }"),
        CompileError);
}

// 異なる構造体型を返そうとするとエラー
TEST(StructTest, ReturnWrongStructTypeThrows) {
    EXPECT_THROW(
        llvm_emit_ir(
            "struct P { var x: int; } "
            "struct Q { var y: int; } "
            "func getP() -> P { var q: Q = Q(1); return q; } "
            "func main() { var r: P = getP(); return r.x; }"),
        CompileError);
}

// struct を返さない関数の結果を struct 変数に代入しようとするとエラー
TEST(StructTest, AssignNonStructFuncToStructVarThrows) {
    EXPECT_THROW(
        llvm_emit_ir(
            "struct Point { var x: int; } "
            "func getInt() { return 42; } "
            "func main() { var p: Point = getInt(); return p.x; }"),
        CompileError);
}

// プリミティブ変数を構造体引数として渡すとエラー
TEST(StructTest, PassPrimitiveAsStructParamThrows) {
    EXPECT_THROW(
        llvm_emit_ir(
            "struct Point { var x: int; } "
            "func getX(p: Point) { return p.x; } "
            "func main() { var a: int = 5; return getX(a); }"),
        CompileError);
}

// struct を返す関数の return 文にプリミティブ変数を使うとエラー
TEST(StructTest, ReturnPrimitiveFromStructFuncThrows) {
    EXPECT_THROW(
        llvm_emit_ir(
            "struct Point { var x: int; } "
            "func getP() -> Point { var n: int = 1; return n; } "
            "func main() { var r: Point = getP(); return r.x; }"),
        CompileError);
}
