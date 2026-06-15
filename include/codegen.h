#pragma once
#include "ast.h"
#include "type_system.h"
#include "vm.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <stdexcept>

struct LocalVar {
    std::string name;
    TypeInfo type;
    int index;
};

struct CodegenContext {
    std::string funcName;
    std::vector<LocalVar> locals;
    // struct context (for method codegen)
    std::string structName; // empty if not in a method
    int thisLocalIndex = -1;
    
    int findLocal(const std::string& name) const;
    int addLocal(const std::string& name, const TypeInfo& type);
    TypeInfo localType(const std::string& name) const;
};

class Codegen {
public:
    explicit Codegen(TypeSystem& ts);
    
    void generate(ASTNode* program);
    VM buildVM();

private:
    TypeSystem& typeSystem;
    std::unordered_map<std::string, std::vector<Instruction>> functions;
    
    // Current function being compiled
    std::vector<Instruction>* currentCode = nullptr;
    CodegenContext* currentCtx = nullptr;
    
    void collectStructs(ASTNode* program);
    void generateFunction(ASTNode* funcDecl);
    void generateMethod(const std::string& structName, ASTNode* methodNode);
    void generateConstructor(const std::string& structName, ASTNode* ctorNode);
    
    void generateBlock(ASTNode* block, CodegenContext& ctx);
    void generateStatement(ASTNode* stmt, CodegenContext& ctx);
    void generateReturn(ASTNode* node, CodegenContext& ctx);
    void generateIf(ASTNode* node, CodegenContext& ctx);
    void generateWhile(ASTNode* node, CodegenContext& ctx);
    void generateFor(ASTNode* node, CodegenContext& ctx);
    void generateVarDecl(ASTNode* node, CodegenContext& ctx);
    void generateExprStmt(ASTNode* node, CodegenContext& ctx);
    
    // Expression codegen - returns type of expression
    TypeInfo generateExpr(ASTNode* node, CodegenContext& ctx);
    TypeInfo generateAssign(ASTNode* node, CodegenContext& ctx);
    TypeInfo generateBinary(ASTNode* node, CodegenContext& ctx);
    TypeInfo generateCall(ASTNode* node, CodegenContext& ctx);
    TypeInfo generateMethodCall(ASTNode* node, CodegenContext& ctx);
    TypeInfo generateMemberAccess(ASTNode* node, CodegenContext& ctx);
    TypeInfo generateIdentifier(ASTNode* node, CodegenContext& ctx);
    TypeInfo generateThis(ASTNode* node, CodegenContext& ctx);
    
    // Helper to emit instruction
    void emit(Instruction instr);
    void emitPushInt(long long v);
    size_t emitJmp(OpCode op);
    void patchJmp(size_t idx);
    
    // Struct method name mangling
    std::string mangledMethodName(const std::string& structName, const std::string& methodName);
    std::string mangledCtorName(const std::string& structName);
    
    // Builtin function names
    bool isBuiltin(const std::string& name) const;
};
