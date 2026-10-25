#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include "../include/parser.hpp"
#include "../include/lexer.hpp"
#include "../include/cpu.hpp"

static vector<Function *> parse_methods(const string &source) {
    Lexer lexer; Parser parser;
    auto tokens = lexer.lex(source);
    return parser.parse(tokens);
}

static int run_methods_vm(const string &source) {
    auto program = parse_methods(source);
    map<string, int> vars, functions;
    vector<Code> codes;
    for (auto function : program) function->compile(codes, vars, functions, 4);
    codes.insert(codes.begin(), Code::makeCode(Code::CALL, functions.at("main"), 0));
    codes.insert(codes.begin() + 1, Code::makeCode(Code::PUSHR, 2, 0));
    codes.insert(codes.begin() + 2, Code::makeCode(Code::POP, 2, 0));
    codes.insert(codes.begin() + 3, Code::makeCode(Code::EXIT, 0, 0));
    CPU cpu; cpu.set(codes); return cpu.exe();
}

static int run_methods_native(const string &source) {
    auto program = parse_methods(source);
    char ir[] = "/tmp/dorothy_methods_XXXXXX.ll";
    char binary[] = "/tmp/dorothy_methods_XXXXXX";
    close(mkstemps(ir, 3)); close(mkstemp(binary));
    std::ofstream out(ir);
    LLVMGenCtx ctx(out);
    for (auto function : program) if (!function->isImport()) ctx.defined_funcs.insert(function->getName());
    for (auto function : program) function->llvm_pre_register(ctx);
    for (auto function : program) function->llvm_emit(ctx);
    out.close();
    string errors = string(binary) + ".err";
    int compiled = system(("clang -Wno-override-module " + string(ir) + " -o " + binary + " 2>" + errors).c_str());
    if (compiled) {
        std::ifstream input(errors); std::stringstream log; log << input.rdbuf();
        remove(ir); remove(binary); remove(errors.c_str());
        throw std::runtime_error(log.str());
    }
    int status = system(binary);
    remove(ir); remove(binary); remove(errors.c_str());
    if (!WIFEXITED(status)) throw std::runtime_error("method sample terminated abnormally");
    return WEXITSTATUS(status);
}

static const string recursive_counter =
    "struct Counter { var calls: int; constructor() { this.calls = 0; } "
    "func factorial(n: int) -> int { this.calls = this.calls + 1; "
    "if(n <= 1) { return 1; } var saved = n; return saved * this.factorial(n - 1); } "
    "func even(n: int) -> int { this.calls = this.calls + 1; if(n == 0) { return 1; } return this.odd(n - 1); } "
    "func odd(n: int) -> int { this.calls = this.calls + 1; if(n == 0) { return 0; } return this.even(n - 1); } } ";

TEST(RecursiveMethodTest, StructDirectRecursionBothBackends) {
    auto source = recursive_counter + "func main() -> int { var c = Counter(); return c.factorial(5) + c.calls; }";
    EXPECT_EQ(run_methods_vm(source), 125);
    EXPECT_EQ(run_methods_native(source), 125);
}

TEST(RecursiveMethodTest, StructMutualRecursionAndIndependentReceivers) {
    auto source = recursive_counter + "func main() -> int { var a = Counter(); var b = Counter(); "
        "var answer = 2 + a.even(8) + b.odd(5); return answer + a.calls * 10 + b.calls; }";
    EXPECT_EQ(run_methods_vm(source), 100);
    EXPECT_EQ(run_methods_native(source), 100);
}

TEST(RecursiveMethodTest, StructFramesPreserveLocalsAndPendingExpressionValues) {
    auto source = recursive_counter + "func main() -> int { var n = 7; var c = Counter(); "
        "var answer = n + c.factorial(4) * c.factorial(3); return answer + n; }";
    EXPECT_EQ(run_methods_vm(source), 158);
    EXPECT_EQ(run_methods_native(source), 158);
}

TEST(RecursiveMethodTest, StructRecursionBaseCases) {
    auto source = recursive_counter + "func main() -> int { var c = Counter(); "
        "return c.even(0) + c.odd(0) + c.factorial(0) + c.calls; }";
    EXPECT_EQ(run_methods_vm(source), 5);
    EXPECT_EQ(run_methods_native(source), 5);
}

TEST(RecursiveMethodTest, NativeInheritedSuperRecursion) {
    EXPECT_EQ(run_methods_native(
        "class Base { var calls: int; constructor() { this.calls = 0; } "
        "func sum(n: int) -> int { this.calls = this.calls + 1; if(n == 0) { return 0; } return n + this.sum(n - 1); } } "
        "class Child: Base { constructor() { super(); } override func sum(n: int) -> int { return super.sum(n); } } "
        "func main() -> int { var a = Child(); var b = Child(); return a.sum(5) + a.calls + b.calls; }"), 21);
}

TEST(RecursiveMethodTest, InvalidStructMethodCallsAreRejected) {
    EXPECT_THROW(parse_methods("struct S { func value(n: int) -> int { return n; } } "
        "func main() -> int { var s = S(); return s.value(); }"), CompileError);
    EXPECT_THROW(parse_methods("struct S {} func main() -> int { var s = S(); return s.missing(); }"), CompileError);
    EXPECT_THROW(parse_methods("struct S { func f() -> int { return this.f(); } "
        "func f() -> int { return 0; } }"), ParseError);
}

TEST(RecursiveMethodTest, NullableContractsApplyInsideStructMethods) {
    EXPECT_THROW(parse_methods("class Node { var x: int; } struct S { "
        "func unsafe(n: Node?) -> int { return n.x; } }"), CompileError);
}

TEST(RecursiveMethodTest, FloatingStructRecursionUsesNativeDoubleABI) {
    const string definitions = "struct Sum { func value(n: int) -> double { "
        "if(n == 0) { return 0.5; } return this.value(n - 1) + 0.5; } } ";
    EXPECT_EQ(run_methods_native(definitions +
        "func main() -> int { var s = Sum(); if(s.value(4) > 2.4) { return 42; } return 0; }"), 42);
    float expected = 3.0f; int bits; std::memcpy(&bits, &expected, sizeof(bits));
    EXPECT_EQ(run_methods_vm(definitions +
        "func main() { var s = Sum(); return s.value(4) + s.value(0); }"), bits);
}

TEST(RecursiveMethodTest, ConstructorInsideMethodRestoresCallerReceiverAndBindings) {
    const string source = "struct S { var x: int; constructor(x: int) { this.x = x; } "
        "func value(x: int) -> int { var other = S(x); return this.x + other.x + x; } } "
        "func main() -> int { var s = S(3); return s.value(7); }";
    EXPECT_EQ(run_methods_vm(source), 17);
    EXPECT_EQ(run_methods_native(source), 17);
}

TEST(RecursiveMethodTest, MethodConstructorArgumentsUseOuterReceiver) {
    const string source = "struct S { var x: int; constructor(x: int) { this.x = x; } "
        "func value() -> int { var other = S(this.x + 1); return this.x + other.x; } } "
        "func main() -> int { var s = S(3); return s.value(); }";
    EXPECT_EQ(run_methods_vm(source), 7);
    EXPECT_EQ(run_methods_native(source), 7);
}

TEST(RecursiveMethodTest, NativeClassMethodCanConstructIndependentLocalStruct) {
    EXPECT_EQ(run_methods_native("struct S { var x: int; constructor(x: int) { this.x = x; } } "
        "class C { var x: int; constructor(x: int) { this.x = x; } "
        "func value() -> int { var s = S(7); return s.x + this.x; } } "
        "func main() -> int { var c = C(3); return c.value(); }"), 10);
}

TEST(RecursiveMethodTest, VmRejectsUnsupportedImplicitFloatingRepresentations) {
    EXPECT_THROW(run_methods_vm("struct S { func value() -> double { return 1; } } "
        "func main() { var s = S(); return s.value(); }"), CompileError);
    EXPECT_THROW(run_methods_vm("struct S { func value(x: double) -> double { return x; } } "
        "func main() { var s = S(); return s.value(1); }"), CompileError);
    EXPECT_EQ(run_methods_native("struct S { func value(x: double) -> double { return x; } } "
        "func main() -> int { var s = S(); if(s.value(1) > 0.9) { return 42; } return 0; }"), 42);
}

TEST(RecursiveMethodTest, NativeReferenceReturnsTransferOwnership) {
    const string source = "class Node { var x: int; constructor(x: int) { this.x = x; } } "
        "struct S { var node: Node?; constructor(n: Node) { this.node = n; } "
        "func get() -> Node? { return this.node; } } "
        "func main() -> int { var s = S(Node(7)); var n = s.get(); s.get(); "
        "if(n == null) { return 0; } return n.x; }";
    EXPECT_EQ(run_methods_native(source), 7);
    auto program = parse_methods(source);
    std::ostringstream out; LLVMGenCtx ctx(out);
    for (auto function : program) if (!function->isImport()) ctx.defined_funcs.insert(function->getName());
    for (auto function : program) function->llvm_pre_register(ctx);
    for (auto function : program) function->llvm_emit(ctx);
    string ir = out.str(); auto main = ir.substr(ir.find("define i32 @main"));
    EXPECT_NE(main.find("unused.method.ptr"), string::npos);
    EXPECT_EQ(main.find("call void @_dorothy_retain(ptr %init.ptr"), string::npos);
}

TEST(NestedStructReceiverTest, DeepPathAndSiblingStorageBothBackends) {
    auto source = recursive_counter +
        "struct Branch { var padding: int; var selected: Counter; var sibling: Counter; } "
        "struct Root { var padding: int; var branch: Branch; var sibling: Counter; } "
        "func main() -> int { var root = Root(); root.branch.sibling.calls = 3; root.sibling.calls = 5; "
        "var value = 7 + root.branch.selected.factorial(4); "
        "return value + root.branch.selected.calls + root.branch.sibling.calls + root.sibling.calls; }";
    EXPECT_EQ(run_methods_vm(source), 43);
    EXPECT_EQ(run_methods_native(source), 43);
}

TEST(NestedStructReceiverTest, ThisFieldSelectsNestedReceiverBothBackends) {
    auto source = recursive_counter +
        "struct Branch { var padding: int; var selected: Counter; var sibling: Counter; "
        "func run(n: int) -> int { return this.selected.factorial(n) + this.sibling.calls; } } "
        "struct Root { var padding: int; var branch: Branch; var sibling: Counter; "
        "func run(n: int) -> int { return this.branch.run(n) + this.sibling.calls; } } "
        "func main() -> int { var root = Root(); root.branch.sibling.calls = 3; root.sibling.calls = 5; "
        "var value = 7 + root.run(4); return value + root.branch.selected.calls; }";
    EXPECT_EQ(run_methods_vm(source), 43);
    EXPECT_EQ(run_methods_native(source), 43);
}

TEST(NestedStructReceiverTest, IndependentOuterValuesKeepIndependentNestedInstances) {
    auto source = recursive_counter +
        "struct Root { var padding: int; var inner: Counter; } "
        "func main() -> int { var a = Root(); var b = Root(); "
        "var result = a.inner.factorial(3) + b.inner.factorial(2); "
        "return result + a.inner.calls * 10 + b.inner.calls; }";
    EXPECT_EQ(run_methods_vm(source), 40);
    EXPECT_EQ(run_methods_native(source), 40);
}

TEST(NestedStructReceiverTest, ClassOwnedStructMethodsUseOriginalStorage) {
    EXPECT_EQ(run_methods_native(recursive_counter +
        "struct Branch { var padding: int; var selected: Counter; var sibling: Counter; "
        "func run(n: int) -> int { return this.selected.factorial(n); } } "
        "class Holder { var branch: Branch; var other: Branch; "
        "func run(n: int) -> int { return this.branch.run(n); } } "
        "func main() -> int { var h = Holder(); var alias = h; h.other.sibling.calls = 9; "
        "var result = alias.run(4); return result + h.branch.selected.calls + h.other.sibling.calls; }"), 37);
}

TEST(NestedStructReceiverTest, InvalidNestedPathsFailBeforeExecution) {
    for (const auto &suffix : {
        "return r.inner.missing();",
        "return r.missing.factorial(2);",
        "return r.padding.factorial(2);",
        "return r.inner.factorial();"}) {
        auto source = recursive_counter +
            "struct Root { var padding: int; var inner: Counter; } "
            "func main() -> int { var r = Root(); " + suffix + " }";
        EXPECT_THROW(run_methods_vm(source), CompileError) << suffix;
        EXPECT_THROW(run_methods_native(source), CompileError) << suffix;
    }
}

TEST(NestedStructReceiverTest, MethodLocalNestedStructCopyHasIndependentValueStorage) {
    auto source = recursive_counter +
        "struct Root { var padding: int; var inner: Counter; "
        "func run() -> int { var copy: Counter = this.inner; "
        "var value = copy.factorial(3); return value + copy.calls + this.inner.calls; } } "
        "func main() -> int { var root = Root(); return root.run(); }";
    EXPECT_EQ(run_methods_vm(source), 9);
    EXPECT_EQ(run_methods_native(source), 9);
}

TEST(NestedStructReceiverTest, FloatingRecursiveResultsKeepNestedReceiverType) {
    const string definitions =
        "struct Fraction { func value(n: int) -> double { "
        "if(n == 0) { return 0.5; } return this.value(n - 1) + 0.5; } } "
        "struct Box { var padding: int; var fraction: Fraction; } ";
    float expected = 3.0f; int bits;
    std::memcpy(&bits, &expected, sizeof(bits));
    EXPECT_EQ(run_methods_vm(definitions +
        "func main() { var b = Box(); return b.fraction.value(4) + b.fraction.value(0); }"), bits);
    EXPECT_EQ(run_methods_native(definitions +
        "func main() -> int { var b = Box(); "
        "if(b.fraction.value(4) + b.fraction.value(0) > 2.9) { return 42; } return 0; }"), 42);
}

TEST(NestedStructReceiverTest, FirstLeafIsZeroBeforeRecursiveCall) {
    const string source =
        "struct Counter { var calls: int; func total(n: int) -> int { "
        "this.calls = this.calls + 1; if(n == 0) { return 0; } return n + this.total(n - 1); } } "
        "struct Inner { var counter: Counter; var sibling: int; } "
        "struct Outer { var inner: Inner; var other: Counter; } "
        "func main() -> int { var outer = Outer(); outer.inner.sibling = 7; "
        "var sum = 10 + outer.inner.counter.total(4); "
        "return sum + outer.inner.counter.calls + outer.inner.sibling + outer.other.calls; }";
    EXPECT_EQ(run_methods_vm(source), 32);
    EXPECT_EQ(run_methods_native(source), 32);
}

TEST(NestedStructReceiverTest, IntegralFloatingLiteralsRemainValidLlvmOperands) {
    const string source =
        "struct Scale { func value(x: double) -> double { return x * 1.5; } } "
        "struct Box { var scale: Scale; } func main() -> int { var b = Box(); "
        "if(b.scale.value(2.0) == 3.0) { return 42; } return 0; }";
    EXPECT_EQ(run_methods_vm(source), 42);
    EXPECT_EQ(run_methods_native(source), 42);
}
