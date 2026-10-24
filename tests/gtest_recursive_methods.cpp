#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
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
