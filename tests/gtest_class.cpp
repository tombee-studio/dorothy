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
        "    constructor(x: int, y: int) {\n"
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
        "    constructor(id: int) { this.id = id; }\n"
        "    func getId() -> int { return this.id; }\n"
        "}\n"
        "class Derived: Base {\n"
        "    var extra: int;\n"
        "    constructor(id: int, extra: int) {\n"
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
        "    constructor(side: int) { this.side = side; }\n"
        "    override func area() -> int {\n"
        "        return this.side * this.side;\n"
        "    }\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_NO_THROW(parser.parse(tokens));
}

TEST(ClassTest, ClassConstructorNoFuncKeyword) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class MyClass {\n"
        "    var val: int;\n"
        "    constructor(v: int) {\n"
        "        this.val = v;\n"
        "    }\n"
        "}\n"
        "func main() -> int {\n"
        "    var m: MyClass = MyClass(42);\n"
        "    return m.val;\n"
        "}\n"
    );
    EXPECT_NO_THROW(parser.parse(tokens));
}

// ==========================================
// 2. パーサーレベル（異常系）
// ==========================================

TEST(ClassTest, ClassConstructorDuplicateThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class MyClass {\n"
        "    var val: int;\n"
        "    constructor(v: int) { this.val = v; }\n"
        "    constructor() { this.val = 0; }\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ClassTest, ClassConstructorFuncKeywordThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class MyClass {\n"
        "    var val: int;\n"
        "    func constructor(v: int) { this.val = v; }\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

TEST(ClassTest, ClassConstructorFuncClassNameThrows) {
    Lexer lexer;
    Parser parser;
    auto tokens = lexer.lex(
        "class MyClass {\n"
        "    var val: int;\n"
        "    func MyClass(v: int) { this.val = v; }\n"
        "}\n"
        "func main() -> int { return 0; }\n"
    );
    EXPECT_THROW(parser.parse(tokens), ParseError);
}

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
        "    constructor(side: int) { this.side = side; }\n"
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
        "    constructor() {}\n"
        "}\n"
        "class Derived: Base {\n"
        "    constructor() {}\n"
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
        "    constructor() {}\n"
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
        "    constructor(age: int) { this.age = age; }\n"
        "    func speak() -> int { return 42; }\n"
        "}\n"
        "func main() -> int {\n"
        "    var a: Animal = Animal(5);\n"
        "    return a.speak();\n"
        "}\n"
    );
    EXPECT_NE(ir.find("declare ptr @malloc(i64)"), std::string::npos);
    EXPECT_NE(ir.find("declare void @free(ptr)"), std::string::npos);
    EXPECT_NE(ir.find("define void @_dorothy_retain(ptr %obj)"), std::string::npos);
    EXPECT_NE(ir.find("define void @_dorothy_release(ptr %obj)"), std::string::npos);
    EXPECT_NE(ir.find("%class.Animal = type { ptr, i64, i32 }"), std::string::npos);
    EXPECT_NE(ir.find("@vtable.Animal = global [2 x ptr] [ptr @Animal.destructor, ptr @Animal.speak]"), std::string::npos);
    EXPECT_NE(ir.find("define void @Animal.destructor(ptr %this)"), std::string::npos);
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
        "    constructor(x: int, y: int) {\n"
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
        "    constructor(b: int) { this.base = b; }\n"
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
        "    constructor(v: int) { this.value = v; }\n"
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
        "    constructor(c: int) { this.count = c; }\n"
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
        "    constructor(age: int) { this.age = age; }\n"
        "    func getAge() -> int { return this.age; }\n"
        "}\n"
        "class Employee: Person {\n"
        "    var salary: int;\n"
        "    constructor(age: int, salary: int) {\n"
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
        "    constructor() {}\n"
        "    func compute() -> int { return 10; }\n"
        "}\n"
        "class Derived: Base {\n"
        "    constructor() {}\n"
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
        "    constructor(w: int, h: int) { this.w = w; this.h = h; }\n"
        "    override func area() -> int { return this.w * this.h; }\n"
        "}\n"
        "class Square: Shape {\n"
        "    var side: int;\n"
        "    constructor(side: int) { this.side = side; }\n"
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
        "    constructor(a: int) { this.a = a; }\n"
        "    func val() -> int { return this.a; }\n"
        "}\n"
        "class Level2: Level1 {\n"
        "    var b: int;\n"
        "    constructor(a: int, b: int) {\n"
        "        this.a = a;\n"
        "        this.b = b;\n"
        "    }\n"
        "    override func val() -> int { return this.a + this.b; }\n"
        "}\n"
        "class Level3: Level2 {\n"
        "    var c: int;\n"
        "    constructor(a: int, b: int, c: int) {\n"
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

// ==========================================
// 5. 参照カウンタ方式（ARC）・メモリ解放テスト
// ==========================================

// 再代入時の旧インスタンス解放と新インスタンス保持
TEST(ClassTest, ExecReassignmentCleanup) {
    std::string src =
        "class Node {\n"
        "    var val: int;\n"
        "    constructor(v: int) { this.val = v; }\n"
        "}\n"
        "func main() -> int {\n"
        "    var a: Node = Node(10);\n"
        "    var b: Node = Node(20);\n"
        "    a = b;\n" // a now shares Node(20), Node(10) is freed
        "    return a.val + b.val;\n" // 20 + 20 = 40
        "}\n";
    EXPECT_EQ(run_llvm(src), 40);
}

// 自己代入の安全性（早期解放されないこと）
TEST(ClassTest, ExecSelfAssignmentSafe) {
    std::string src =
        "class Node {\n"
        "    var val: int;\n"
        "    constructor(v: int) { this.val = v; }\n"
        "}\n"
        "func main() -> int {\n"
        "    var a: Node = Node(99);\n"
        "    a = a;\n" // self-assignment must not cause premature free
        "    return a.val;\n" // 99
        "}\n";
    EXPECT_EQ(run_llvm(src), 99);
}

// クラスメンバ（親オブジェクトが子オブジェクトを保持）の再帰的解放
TEST(ClassTest, ExecNestedClassMemberRecursiveCleanup) {
    std::string src =
        "class Child {\n"
        "    var id: int;\n"
        "    constructor(i: int) { this.id = i; }\n"
        "}\n"
        "class Parent {\n"
        "    var child: Child;\n"
        "    constructor(c: Child) { this.child = c; }\n"
        "}\n"
        "func test_nested() -> int {\n"
        "    var c: Child = Child(50);\n"
        "    var p: Parent = Parent(c);\n"
        "    return p.child.id;\n"
        "}\n"
        "func main() -> int {\n"
        "    return test_nested();\n" // 50
        "}\n";
    EXPECT_EQ(run_llvm(src), 50);
}

// 関数からのインスタンス返却と所有権移譲
TEST(ClassTest, ExecFunctionReturnOwnershipTransfer) {
    std::string src =
        "class Item {\n"
        "    var price: int;\n"
        "    constructor(p: int) { this.price = p; }\n"
        "}\n"
        "func make_item(p: int) -> Item {\n"
        "    var it: Item = Item(p);\n"
        "    return it;\n"
        "}\n"
        "func main() -> int {\n"
        "    var item: Item = make_item(150);\n"
        "    return item.price;\n" // 150
        "}\n";
    EXPECT_EQ(run_llvm(src), 150);
}

// 多態性（基底クラス参照経由）での派生クラスデストラクタ呼び出し
TEST(ClassTest, ExecPolymorphicDestructorWithFields) {
    std::string src =
        "abstract class BaseObj {\n"
        "    abstract func getVal() -> int;\n"
        "}\n"
        "class ChildObj {\n"
        "    var score: int;\n"
        "    constructor(s: int) { this.score = s; }\n"
        "}\n"
        "class DerivedObj: BaseObj {\n"
        "    var child: ChildObj;\n"
        "    constructor(c: ChildObj) { this.child = c; }\n"
        "    override func getVal() -> int { return this.child.score; }\n"
        "}\n"
        "func calc(b: BaseObj) -> int {\n"
        "    return b.getVal();\n"
        "}\n"
        "func main() -> int {\n"
        "    var c: ChildObj = ChildObj(77);\n"
        "    var d: DerivedObj = DerivedObj(c);\n"
        "    var b: BaseObj = d;\n"
        "    return calc(b);\n" // 77
        "}\n";
    EXPECT_EQ(run_llvm(src), 77);
}

// ループ内で大量に Entity を生成・破棄（ユーザーのユースケース検証）
TEST(ClassTest, ExecMassiveDynamicCreationInLoop) {
    std::string src =
        "class Bullet {\n"
        "    var x: int;\n"
        "    var y: int;\n"
        "    var damage: int;\n"
        "    constructor(x: int, y: int, d: int) {\n"
        "        this.x = x;\n"
        "        this.y = y;\n"
        "        this.damage = d;\n"
        "    }\n"
        "}\n"
        "func main() -> int {\n"
        "    var total: int = 0;\n"
        "    var i: int = 0;\n"
        "    while (i < 10000) {\n"
        "        var b: Bullet = Bullet(i, i * 2, 5);\n"
        "        total = total + b.damage;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    // total = 10000 * 5 = 50000 -> 50000 % 256 = 80 (or return total / 1000 = 50)\n"
        "    return total / 1000;\n" // 50
        "}\n";
    EXPECT_EQ(run_llvm(src), 50);
}

// super binds to the lexical parent and bypasses virtual dispatch.
TEST(ClassSuperTest, ConstructorAndMethodChain) {
    EXPECT_EQ(run_llvm(
        "class Base { var x: int; constructor(x: int) { this.x = x; } "
        "func value(n: int) -> int { return this.x + n; } } "
        "class Child: Base { var y: int; "
        "constructor(x: int, y: int) { super(x); this.y = y; } "
        "override func value(n: int) -> int { return super.value(n) + this.y; } } "
        "class Leaf: Child { constructor(x: int, y: int) { super(x, y); } "
        "override func value(n: int) -> int { return super.value(n) + 1; } } "
        "func main() -> int { var c = Leaf(10, 20); return c.value(3); }"), 34);
}

TEST(ClassSuperTest, InheritedMethodUsesDefiningClass) {
    EXPECT_EQ(run_llvm(
        "class A { func value() -> int { return 10; } } "
        "class B: A {} "
        "class C: B { override func value() -> int { return super.value() + 2; } } "
        "func main() -> int { var c = C(); return c.value(); }"), 12);
}

TEST(ClassSuperTest, BaseCallPreservesVirtualDispatchInsideParent) {
    EXPECT_EQ(run_llvm(
        "class A { func value() -> int { return this.extra() + 1; } "
        "func extra() -> int { return 2; } } "
        "class B: A { override func value() -> int { return super.value() + 3; } "
        "override func extra() -> int { return 10; } } "
        "func main() -> int { var b = B(); return b.value(); }"), 14);
}

TEST(ClassSuperTest, DefaultBaseConstructorAndStringArguments) {
    EXPECT_EQ(run_llvm(
        "class A {} class B: A { constructor() { super(); } } "
        "class Text { var text: string; constructor(text: string) { this.text = text; } "
        "func length() -> int { if(this.text == \"hello\") { return 5; } return 0; } } "
        "class Message: Text { constructor() { super(\"hello\"); } "
        "override func length() -> int { return super.length(); } } "
        "func main() -> int { var b = B(); var m = Message(); return m.length(); }"), 5);
}

TEST(ClassSuperTest, InvalidContexts) {
    for (const auto &source : {
        "func main() -> int { return super.value(); }",
        "class A { constructor() { super(); } }",
        "class A {} class B: A { func f() -> int { super(); return 0; } }",
        "class A {} class B: A { constructor() { super; } }",
        "struct A { constructor() { super(); } }"}) {
        Lexer lexer; Parser parser;
        auto tokens = lexer.lex(source);
        EXPECT_THROW(parser.parse(tokens), ParseError) << source;
    }
}

TEST(ClassSuperTest, InvalidTargetsAndArgumentCounts) {
    for (const auto &source : {
        "class A {} class B: A { constructor() { super(1); } } func main() -> int { return 0; }",
        "class A { constructor(x: int) {} } class B: A { constructor() { super(); } } func main() -> int { return 0; }",
        "class A {} class B: A { func f() -> int { return super.missing(); } } func main() -> int { return 0; }",
        "class A { func f(x: int) -> int { return x; } } class B: A { override func f(x: int) -> int { return super.f(); } } func main() -> int { return 0; }",
        "abstract class A { abstract func f() -> int; } class B: A { override func f() -> int { return super.f(); } } func main() -> int { return 0; }"}) {
        EXPECT_THROW(llvm_emit_ir(source), CompileError) << source;
    }
}

TEST(ClassSuperTest, DoubleReturnPreservesFractionAcrossParentChain) {
    EXPECT_EQ(run_llvm(
        "class Base { func value() -> double { return 1.25; } } "
        "class Child: Base { override func value() -> double { return super.value() * 2; } } "
        "class Leaf: Child { override func value() -> double { return super.value() + 0.75; } } "
        "func main() -> int { var c = Leaf(); return c.value() * 20; }"), 65);
}

TEST(ClassSuperTest, FloatConstructorAndMethodConversions) {
    EXPECT_EQ(run_llvm(
        "class Base { var value: float; constructor(value: float) { this.value = value; } "
        "func amount() -> float { return this.value; } } "
        "class Child: Base { constructor(value: double) { super(value); } "
        "override func amount() -> float { return super.amount() + 0.5; } } "
        "func main() -> int { var c = Child(1.25); return c.amount() * 100; }"), 175);
}

TEST(ClassSuperTest, DoubleParentCallRetainsVirtualDispatch) {
    EXPECT_EQ(run_llvm(
        "class Base { func part() -> double { return 1.5; } "
        "func value() -> double { return this.part() + 0.25; } } "
        "class Child: Base { override func part() -> double { return 2.5; } "
        "override func value() -> double { return super.value() + 0.25; } } "
        "func main() -> int { var c = Child(); return c.value() * 10; }"), 30);
}

TEST(ClassSuperTest, FloatingMethodCanWidenIntegerAndFallThrough) {
    EXPECT_EQ(run_llvm(
        "class Base { func integer() -> double { return 2; } "
        "func empty() -> float {} } "
        "class Child: Base { func value() -> int { return super.integer() * 10 + super.empty(); } } "
        "func ordinary() -> int { return 3; } "
        "func main() -> int { var c = Child(); return c.value() + ordinary(); }"), 23);
}

TEST(ClassSuperTest, FloatingParentUsesDoubleReturnABI) {
    auto ir = llvm_emit_ir(
        "class Base { func value() -> double { return 1.25; } } "
        "class Child: Base { override func value() -> double { return super.value() + 0.5; } } "
        "func main() -> int { var c = Child(); return c.value(); }");
    EXPECT_NE(ir.find("define double @Base.value(ptr %this)"), std::string::npos);
    EXPECT_NE(ir.find("call double (ptr) @Base.value("), std::string::npos);
}

TEST(ClassSuperTest, RejectsIncompatibleFloatingOverrideABI) {
    for (const auto &source : {
        "class Base { func value() -> double { return 1.25; } } class Child: Base { override func value() -> int { return 2; } }",
        "class Base { func value() -> int { return 1; } } class Child: Base { override func value() -> float { return 2.5; } }",
        "class Base { func value(x: double) -> int { return 1; } } class Child: Base { override func value(x: float) -> int { return 2; } }"}) {
        Lexer lexer; Parser parser;
        auto tokens = lexer.lex(source);
        EXPECT_THROW(parser.parse(tokens), ParseError) << source;
    }
}
TEST(NestedReceiverTest, ClassInsideDeepStructPathMutatesOnlySelectedReceiver) {
    EXPECT_EQ(run_llvm(
        "class Counter { var value: int; constructor(v: int) { this.value = v; } "
        "func add(n: int) -> int { this.value = this.value + n; return this.value; } } "
        "struct Leaf { var counter: Counter; constructor() { this.counter = Counter(5); } } "
        "struct Branch { var leaf: Leaf; constructor() { this.leaf.counter = Counter(5); } } "
        "struct Root { var branch: Branch; var sibling: Counter; constructor() { "
        "this.branch.leaf.counter = Counter(5); this.sibling = Counter(99); } } "
        "func main() -> int { var root = Root(); var changed = root.branch.leaf.counter.add(2); "
        "return changed + root.sibling.value; }"), 106);
}

TEST(NestedReceiverTest, NestedThisReceiverPreservesInheritanceAndLexicalSuper) {
    EXPECT_EQ(run_llvm(
        "class Base { var value: int; constructor(v: int) { this.value = v; } "
        "func add(n: int) -> int { this.value = this.value + n; return this.value; } } "
        "class Child: Base { constructor(v: int) { super(v); } "
        "override func add(n: int) -> int { return super.add(n) + 10; } } "
        "class Holder { var selected: Child; var sibling: Base; constructor() { "
        "this.selected = Child(3); this.sibling = Base(80); } "
        "func update(n: int) -> int { return this.selected.add(n) + this.sibling.value; } } "
        "func main() -> int { var h = Holder(); return h.update(2); }"), 95);
}

TEST(NestedReceiverTest, NestedNullableReceiversRequireGuards) {
    EXPECT_THROW(llvm_emit_ir(
        "class Counter { func value() -> int { return 1; } } "
        "struct Holder { var counter: Counter?; } "
        "func main() -> int { var h = Holder(); return h.counter.value(); }"), CompileError);
    EXPECT_EQ(run_llvm(
        "class Counter { func value() -> int { return 7; } } "
        "struct Holder { var counter: Counter?; } "
        "func main() -> int { var h = Holder(); h.counter = Counter(); "
        "if(h.counter == null) { return 0; } return h.counter.value(); }"), 7);
}

TEST(NestedReceiverTest, InvalidNestedReceiversReportCompileErrors) {
    for (const auto &source : {
        "struct Inner { var value: int; } struct Outer { var inner: Inner; } "
        "func main() -> int { var o = Outer(); return o.inner.missing.value(); }",
        "struct Inner { var value: int; } struct Outer { var inner: Inner; } "
        "func main() -> int { var o = Outer(); return o.inner.value.method(); }",
        "class Counter { func value(n: int) -> int { return n; } } "
        "struct Inner { var counter: Counter?; } struct Outer { var inner: Inner; } "
        "func main() -> int { var o = Outer(); o.inner.counter = Counter(); "
        "return o.inner.counter.missing(); }",
        "class Counter { func value(n: int) -> int { return n; } } "
        "struct Inner { var counter: Counter?; } struct Outer { var inner: Inner; } "
        "func main() -> int { var o = Outer(); o.inner.counter = Counter(); "
        "return o.inner.counter.value(); }"}) {
        EXPECT_THROW(llvm_emit_ir(source), CompileError) << source;
    }
}

TEST(NestedReceiverTest, ClassOwnedStructStorageSupportsMixedNestedPaths) {
    EXPECT_EQ(run_llvm(
        "class Counter { var value: int; constructor(v: int) { this.value = v; } "
        "func add(n: int) -> int { this.value = this.value + n; return this.value; } } "
        "struct Leaf { var counter: Counter; constructor() { this.counter = Counter(4); } } "
        "struct Branch { var leaf: Leaf; constructor() { this.leaf.counter = Counter(4); } } "
        "class Holder { var selected: Branch; var sibling: Counter; constructor() { "
        "this.selected.leaf.counter = Counter(4); this.sibling = Counter(80); } "
        "func update() -> int { return this.selected.leaf.counter.add(3); } } "
        "func main() -> int { var h = Holder(); var alias = h; "
        "var changed = alias.update(); return changed + h.selected.leaf.counter.value + h.sibling.value; }"), 94);
}

TEST(NestedReceiverTest, RetainedClassLeafSurvivesClassOwnedStructDestruction) {
    EXPECT_EQ(run_llvm(
        "class Counter { var value: int; constructor(v: int) { this.value = v; } "
        "func valueOf() -> int { return this.value; } } "
        "struct Inner { var counter: Counter; constructor() { this.counter = Counter(42); } } "
        "class Holder { var inner: Inner; constructor() { this.inner.counter = Counter(42); } } "
        "func keep() -> Counter { var h = Holder(); var saved = h.inner.counter; return saved; } "
        "func main() -> int { var saved = keep(); return saved.valueOf(); }"), 42);
}

TEST(NestedReceiverTest, WholeClassOwnedStructCopiesAreRejectedClearly) {
    EXPECT_THROW(llvm_emit_ir(
        "struct Inner { var value: int; } class Holder { var inner: Inner; } "
        "func main() -> int { var h = Holder(); var copy: Inner = h.inner; return copy.value; }"), CompileError);
}

TEST(NestedReceiverTest, StructBackingOwnerSurvivesArgumentEvaluation) {
    EXPECT_EQ(run_llvm(
        "struct Counter { var value: int; func add(n: int) -> int { "
        "this.value = this.value + n; return this.value; } } "
        "class Owner { var branch: Counter; constructor() { this.branch.value = 41; } } "
        "class Holder { var owner: Owner?; constructor() { this.owner = Owner(); } "
        "func drop() -> int { this.owner = null; return 1; } } "
        "func main() -> int { var h = Holder(); if(h.owner == null) { return 0; } "
        "return h.owner.branch.add(h.drop()); }"), 42);
}

TEST(NestedReceiverTest, ClassLeafReceiverSurvivesArgumentEvaluation) {
    EXPECT_EQ(run_llvm(
        "class Node { var value: int; constructor() { this.value = 41; } "
        "func add(n: int) -> int { return this.value + n; } } "
        "struct Branch { var node: Node?; } "
        "class Owner { var branch: Branch; constructor() { this.branch.node = Node(); } } "
        "class Holder { var owner: Owner?; constructor() { this.owner = Owner(); } "
        "func drop() -> int { this.owner = null; return 1; } } "
        "func main() -> int { var h = Holder(); if(h.owner == null) { return 0; } "
        "if(h.owner.branch.node == null) { return 0; } return h.owner.branch.node.add(h.drop()); }"), 42);
}

TEST(NestedReceiverTest, StructFieldAssignmentPinsOwnerAcrossRightHandSide) {
    EXPECT_EQ(run_llvm(
        "struct Counter { var value: int; } "
        "class Owner { var branch: Counter; } "
        "class Holder { var owner: Owner?; constructor() { this.owner = Owner(); } "
        "func drop() -> int { this.owner = null; return 41; } } "
        "func main() -> int { var h = Holder(); if(h.owner == null) { return 0; } "
        "var result = (h.owner.branch.value = h.drop()); return result; }"), 41);
}
