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

// LLVM IR を生成して clang でコンパイル・実行し、戻り値（exit code）を取得するヘルパー
static int run_llvm(const std::string& source) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(source);
    auto program = parser.parse(tokens);

    char tmp_ll[] = "/tmp/dorothy_test_class_XXXXXX.ll";
    char tmp_bin[] = "/tmp/dorothy_test_class_bin_XXXXXX";
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

TEST(ClassTest, ParseBasicClass) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "    func Point(x: int, y: int) {\n"
        "        this.x = x;\n"
        "        this.y = y;\n"
        "    }\n"
        "    func sum() -> int {\n"
        "        return this.x + this.y;\n"
        "    }\n"
        "}\n"
        "func main() -> int {\n"
        "    var p: Point = Point(3, 4);\n"
        "    return p.sum();\n"
        "}\n"
    );
    EXPECT_NO_THROW(parser.parse(tokens));
}

TEST(ClassTest, ParseInheritance) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Base {\n"
        "    var id: int;\n"
        "    func Base(id: int) { this.id = id; }\n"
        "    func getId() -> int { return this.id; }\n"
        "}\n"
        "class Derived: Base {\n"
        "    var extra: int;\n"
        "    func Derived(id: int, extra: int) {\n"
        "        this.id = id;\n"
        "        this.extra = extra;\n"
        "    }\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_NO_THROW(parser.parse(tokens));
}

TEST(ClassTest, ParseAbstractClassAndOverride) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "abstract class Shape {\n"
        "    abstract func area() -> int;\n"
        "}\n"
        "class Square: Shape {\n"
        "    var side: int;\n"
        "    func Square(side: int) { this.side = side; }\n"
        "    override func area() -> int {\n"
        "        return this.side * this.side;\n"
        "    }\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_NO_THROW(parser.parse(tokens));
}

// ==========================================
// 2. パーサーレベル（異常系）
// ==========================================

TEST(ClassTest, InstantiateAbstractClassThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "abstract class Shape {\n"
        "    abstract func area() -> int;\n"
        "}\n"
        "func main() -> int {\n"
        "    var s: Shape = Shape();\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ClassTest, UnimplementedAbstractMethodThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "abstract class Shape {\n"
        "    abstract func area() -> int;\n"
        "}\n"
        "class Incomplete: Shape {\n"
        "    var side: int;\n"
        "    func Incomplete(side: int) { this.side = side; }\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ClassTest, InvalidOverrideMethodThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Base {\n"
        "    func Base() {}\n"
        "}\n"
        "class Derived: Base {\n"
        "    func Derived() {}\n"
        "    override func notInBase() -> int { return 1; }\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ClassTest, AbstractMethodInConcreteClassThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Concrete {\n"
        "    abstract func foo() -> int;\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ClassTest, InheritUndefinedClassThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class Derived: NonExistentBase {\n"
        "    func Derived() {}\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

// ==========================================
// 3. LLVM IR 生成検証
// ==========================================

TEST(ClassTest, LLVMEmitClassAndVTable) {
    auto ir = llvm_emit_ir(
        "class Animal {\n"
        "    var age: int;\n"
        "    func Animal(age: int) { this.age = age; }\n"
        "    func speak() -> int { return 42; }\n"
        "}\n"
        "func main() -> int {\n"
        "    var a: Animal = Animal(5);\n"
        "    return a.speak();\n"
        "}\n"
    );
    EXPECT_NE(ir.find("declare ptr @malloc(i64)"), std::string::npos);
    EXPECT_NE(ir.find("%class.Animal = type { ptr, i32 }"), std::string::npos);
    EXPECT_NE(ir.find("@vtable.Animal = global [1 x ptr] [ptr @Animal.speak]"), std::string::npos);
    EXPECT_NE(ir.find("define void @Animal.constructor(ptr %this"), std::string::npos);
    EXPECT_NE(ir.find("define i64 @Animal.speak(ptr %this)"), std::string::npos);
    EXPECT_NE(ir.find("call ptr @malloc("), std::string::npos);
}

// ==========================================
// 4. 実行テスト（ネイティブバイナリ検証）
// ==========================================

// 基本的なクラス、フィールドアクセス、コンストラクタ
TEST(ClassTest, ExecBasicClass) {
    std::string src =
        "class Point {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "    func Point(x: int, y: int) {\n"
        "        this.x = x;\n"
        "        this.y = y;\n"
        "    }\n"
        "}\n"
        "func main() -> int {\n"
        "    var p: Point = Point(10, 25);\n"
        "    return p.x + p.y;\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), 35);
}

// メソッド呼び出し
TEST(ClassTest, ExecClassMethod) {
    std::string src =
        "class Calculator {\n"
        "    var base: int;\n"
        "    func Calculator(b: int) { this.base = b; }\n"
        "    func add(v: int) -> int { return this.base + v; }\n"
        "    func mul(v: int) -> int { return this.base * v; }\n"
        "}\n"
        "func main() -> int {\n"
        "    var c: Calculator = Calculator(7);\n"
        "    return c.add(3) + c.mul(4);\n"
        "}\n";
    EXPECT_EQ(run_llvm(src), (7 + 3) + (7 * 4)); // 10 + 28 = 38
}

// 参照渡しの検証（変数代入での参照共有）
TEST(ClassTest, ExecPassByReferenceAssignment) {
    std::string src =
        "class Box {\n"
        "    var value: int;\n"
        "    func Box(v: int) { this.value = v; }\n"
        "}\n"
        "func main() -> int {\n"
        "    var b1: Box = Box(10);\n"
        "    var b2: Box = b1;\n"
        "    b2.value = 50;\n"
        "    return b1.value;\n" // b1.value should also be 50 because it's reference passed
        "}\n";
    EXPECT_EQ(run_llvm(src), 50);
}

// 参照渡しの検証（関数引数での参照渡し）
TEST(ClassTest, ExecPassByReferenceFunctionArg) {
    std::string src =
        "class Counter {\n"
        "    var count: int;\n"
        "    func Counter(c: int) { this.count = c; }\n"
        "}\n"
        "func increment(c: Counter, amount: int) {\n"
        "    c.count = c.count + amount;\n"
        "}\n"
        "func main() -> int {\n"
        "    var c: Counter = Counter(5);\n"
        "    increment(c, 15);\n"
        "    return c.count;\n" // should be 20
        "}\n";
    EXPECT_EQ(run_llvm(src), 20);
}

// 継承（基底クラスのフィールドとメソッド呼び出し）
TEST(ClassTest, ExecInheritanceBasic) {
    std::string src =
        "class Person {\n"
        "    var age: int;\n"
        "    func Person(age: int) { this.age = age; }\n"
        "    func getAge() -> int { return this.age; }\n"
        "}\n"
        "class Employee: Person {\n"
        "    var salary: int;\n"
        "    func Employee(age: int, salary: int) {\n"
        "        this.age = age;\n"
        "        this.salary = salary;\n"
        "    }\n"
        "    func getSalary() -> int { return this.salary; }\n"
        "}\n"
        "func main() -> int {\n"
        "    var emp: Employee = Employee(30, 70);\n"
        "    return emp.getAge() + emp.getSalary();\n" // 30 + 70 = 100
        "}\n";
    EXPECT_EQ(run_llvm(src), 100);
}

// メソッドオーバーライドと多態性（Polymorphism）
TEST(ClassTest, ExecMethodOverridePolymorphism) {
    std::string src =
        "class Base {\n"
        "    func Base() {}\n"
        "    func compute() -> int { return 10; }\n"
        "}\n"
        "class Derived: Base {\n"
        "    func Derived() {}\n"
        "    override func compute() -> int { return 25; }\n"
        "}\n"
        "func evaluate(b: Base) -> int {\n"
        "    return b.compute();\n"
        "}\n"
        "func main() -> int {\n"
        "    var b: Base = Base();\n"
        "    var d: Derived = Derived();\n"
        "    return evaluate(b) + evaluate(d);\n" // 10 + 25 = 35
        "}\n";
    EXPECT_EQ(run_llvm(src), 35);
}

// 抽象クラスと抽象メソッドの実装
TEST(ClassTest, ExecAbstractClassPolymorphism) {
    std::string src =
        "abstract class Shape {\n"
        "    abstract func area() -> int;\n"
        "}\n"
        "class Rectangle: Shape {\n"
        "    var w: int;\n"
        "    var h: int;\n"
        "    func Rectangle(w: int, h: int) { this.w = w; this.h = h; }\n"
        "    override func area() -> int { return this.w * this.h; }\n"
        "}\n"
        "class Square: Shape {\n"
        "    var side: int;\n"
        "    func Square(side: int) { this.side = side; }\n"
        "    override func area() -> int { return this.side * this.side; }\n"
        "}\n"
        "func getArea(s: Shape) -> int {\n"
        "    return s.area();\n"
        "}\n"
        "func main() -> int {\n"
        "    var r: Rectangle = Rectangle(3, 4);\n"
        "    var sq: Square = Square(5);\n"
        "    return getArea(r) + getArea(sq);\n" // 12 + 25 = 37
        "}\n";
    EXPECT_EQ(run_llvm(src), 37);
}

// 多段継承（A -> B -> C）
TEST(ClassTest, ExecMultiLevelInheritance) {
    std::string src =
        "class Level1 {\n"
        "    var a: int;\n"
        "    func Level1(a: int) { this.a = a; }\n"
        "    func val() -> int { return this.a; }\n"
        "}\n"
        "class Level2: Level1 {\n"
        "    var b: int;\n"
        "    func Level2(a: int, b: int) {\n"
        "        this.a = a;\n"
        "        this.b = b;\n"
        "    }\n"
        "    override func val() -> int { return this.a + this.b; }\n"
        "}\n"
        "class Level3: Level2 {\n"
        "    var c: int;\n"
        "    func Level3(a: int, b: int, c: int) {\n"
        "        this.a = a;\n"
        "        this.b = b;\n"
        "        this.c = c;\n"
        "    }\n"
        "    override func val() -> int { return this.a + this.b + this.c; }\n"
        "}\n"
        "func getVal(l: Level1) -> int {\n"
        "    return l.val();\n"
        "}\n"
        "func main() -> int {\n"
        "    var l1: Level1 = Level1(1);\n"
        "    var l2: Level2 = Level2(1, 10);\n"
        "    var l3: Level3 = Level3(1, 10, 100);\n"
        "    return getVal(l1) + getVal(l2) + getVal(l3);\n" // 1 + 11 + 111 = 123
        "}\n";
    EXPECT_EQ(run_llvm(src), 123);
}
