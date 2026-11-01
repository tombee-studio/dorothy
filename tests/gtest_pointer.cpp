#include <gtest/gtest.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include "../include/parser.hpp"
#include "../include/lexer.hpp"
#include "../include/cpu.hpp"

static vector<Function *> pointer_parse(const string &source) {
    Lexer lexer; Parser parser;
    auto tokens = lexer.lex(source);
    return parser.parse(tokens);
}
static string pointer_ir(const string &source) {
    auto program = pointer_parse(source);
    std::ostringstream out; LLVMGenCtx ctx(out);
    for (auto f : program) if (!f->isImport()) ctx.defined_funcs.insert(f->getName());
    for (auto f : program) f->llvm_pre_register(ctx);
    for (auto f : program) f->llvm_emit(ctx);
    return out.str();
}
static int pointer_vm(const string &source) {
    auto program = pointer_parse(source);
    map<string, int> vars, functions; vector<Code> codes;
    for (auto f : program) f->compile(codes, vars, functions, 4);
    codes.insert(codes.begin(), Code::makeCode(Code::CALL, functions.at("main"), 0));
    codes.insert(codes.begin()+1, Code::makeCode(Code::PUSHR, 2, 0));
    codes.insert(codes.begin()+2, Code::makeCode(Code::POP, 2, 0));
    codes.insert(codes.begin()+3, Code::makeCode(Code::EXIT, 0, 0));
    CPU cpu; cpu.set(codes); return cpu.exe();
}
static int pointer_native(const string &source) {
    auto ir = pointer_ir(source);
    char ll[] = "/tmp/dorothy_pointer_XXXXXX.ll", bin[] = "/tmp/dorothy_pointer_XXXXXX";
    close(mkstemps(ll,3)); close(mkstemp(bin));
    { std::ofstream out(ll); out << ir; }
    string errors = string(bin)+".err";
    int status = system(("clang -Wno-override-module " + string(ll) + " -o " + bin + " 2>" + errors).c_str());
    if (status) {
        std::ifstream in(errors); std::ostringstream message; message << in.rdbuf();
        remove(ll); remove(bin); remove(errors.c_str());
        throw std::runtime_error(message.str());
    }
    status = system(bin);
    remove(ll); remove(bin); remove(errors.c_str());
    if (!WIFEXITED(status)) throw std::runtime_error("pointer sample terminated abnormally");
    return WEXITSTATUS(status);
}
static void pointer_both(const string &source, int expected) {
    EXPECT_EQ(pointer_vm(source), expected);
    EXPECT_EQ(pointer_native(source), expected);
}

TEST(PointerTest, DefaultAndExplicitNullAreIdentical) {
    pointer_both("func main() -> int { var p: pointer; var q: pointer = pointer.null; "
        "if(p != null) { return 1; } if(q != null) { return 2; } "
        "if(pointer.null != null) { return 3; } if(null != pointer.null) { return 4; } "
        "if(p != q) { return 5; } p = null; q = pointer.null; return 42; }",42);
}
TEST(PointerTest, TypedNullSupportsInference) {
    pointer_both("func main() -> int { var p = pointer.null; if(p == null) { return 42; } return 0; }",42);
}
TEST(PointerTest, AddressReadWriteAndReset) {
    pointer_both("func main() -> int { var value: long = 41; var p: pointer = &value; "
        "if(p == null) { return 1; } *p = *p + 1; "
        "if(value != 42) { return 2; } p = pointer.null; if(p == null) { return 42; } return 3; }",42);
}
TEST(PointerTest, FunctionArgumentsAndReturns) {
    pointer_both("func identity(p: pointer) -> pointer { return p; } "
        "func main() -> int { var value: long = 41; var p = identity(&value); "
        "if(p == null) { return 0; } *p = 42; identity(pointer.null); return value; }",42);
}
TEST(PointerTest, StructFieldsAndMethodReturns) {
    pointer_both("struct Holder { var address: pointer; func set(p: pointer) { this.address = p; } "
        "func get() -> pointer { return this.address; } } "
        "func main() -> int { var h = Holder(); if(h.address != null) { return 1; } "
        "var value: long = 41; h.set(&value); var p = h.get(); "
        "if(p == null) { return 2; } *p = 42; return value; }",42);
}
TEST(PointerTest, ClassFieldsAndInheritedPointerMethods) {
    EXPECT_EQ(pointer_native("class Base { var address: pointer; constructor(p: pointer) { this.address = p; } "
        "func get() -> pointer { return this.address; } } "
        "class Child: Base { constructor(p: pointer) { super(p); } "
        "override func get() -> pointer { return super.get(); } } "
        "func main() -> int { var value: long = 41; var h = Child(&value); "
        "var p = h.get(); if(p == null) { return 1; } *p = 42; return value; }"),42);
}
TEST(PointerTest, PointerNullIsAlsoTheNullableClassNull) {
    EXPECT_EQ(pointer_native("class Node { var x: int; } "
        "func main() -> int { var node: Node? = pointer.null; if(node != null) { return 1; } "
        "node = null; if(node == pointer.null) { return 42; } return 2; }"),42);
}
TEST(PointerTest, NativePointerArrayElements) {
    for (const string spelling : {"Array<pointer>", "pointer[]"}) {
        EXPECT_EQ(pointer_native("func main() -> int { var value: long = 41; "
            "var addresses: " + spelling + " = [pointer.null, &value]; "
            "var p = addresses[1]; if(p == null) { return 1; } *p = 42; return value; }"),42);
    }
}

TEST(PointerTest, CPointerReturnAndArguments) {
    EXPECT_EQ(pointer_native("import \"stdlib.h\"; func main() -> int { free(null); free(pointer.null); var p: pointer = malloc(8); "
        "if(p == pointer.null) { return 1; } *p = 42; var value: long = *p; free(p); return value; }"),42);
}
TEST(PointerTest, NullDereferenceAndWrongTypesAreRejected) {
    for (auto source : {
        "func main() -> int { var p: pointer; return *p; }",
        "func main() -> int { var p: pointer = pointer.null; *p = 42; return 0; }",
        "func main() -> int { var p: pointer = 42; return 0; }",
        "func main() -> int { var p: pointer = 2.5; return 0; }",
        "func main() -> int { var p: pointer = \"hello\"; return 0; }"})
        EXPECT_THROW(pointer_parse(source), CompileError) << source;
}
TEST(PointerTest, NullConstantCannotBeAssignedOrUseUnknownMember) {
    EXPECT_THROW(pointer_ir("func main() -> int { pointer.null = null; return 0; }"), CompileError);
    EXPECT_THROW(pointer_parse("func main() -> int { var p: pointer = pointer.unknown; return 0; }"), ParseError);
}
TEST(PointerTest, NativeRepresentationUsesPointerAbi) {
    auto ir = pointer_ir("func identity(p: pointer) -> pointer { return p; } "
        "func main() -> int { var p: pointer; p = identity(pointer.null); if(p == null) { return 0; } return 1; }");
    EXPECT_NE(ir.find("define ptr @identity(ptr"),string::npos);
    EXPECT_NE(ir.find("alloca ptr"),string::npos);
    EXPECT_NE(ir.find("store ptr null"),string::npos);
}

TEST(PointerTest, PointerFieldsAndMissingReturnsDefaultToNull) {
    EXPECT_EQ(pointer_native("class Handle { var address: pointer; "
        "func empty() -> pointer {} func get() -> pointer { return this.address; } } "
        "func empty() -> pointer {} func main() -> int { var h = Handle(); "
        "if(h.get() != null) { return 1; } if(h.empty() != pointer.null) { return 2; } "
        "if(empty() == null) { return 42; } return 3; }"),42);
}
TEST(PointerTest, PointerAbiCannotBeChangedByOverrideOrMain) {
    EXPECT_THROW(pointer_parse("class Base { func get() -> pointer { return null; } } "
        "class Child: Base { override func get() -> long { return 0; } }"),ParseError);
    EXPECT_THROW(pointer_parse("func main() -> pointer { return pointer.null; }"),ParseError);
}
