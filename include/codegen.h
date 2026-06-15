#pragma once
#include "ast.h"
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <stdexcept>

// LLVM IR code generator
class CodeGen {
public:
    explicit CodeGen(ASTNodePtr ast);
    std::string emit();

private:
    ASTNodePtr ast;
    std::ostringstream out;
    int tempCounter;
    int labelCounter;
    int varCounter;

    struct VarInfo {
        std::string llvmName;   // e.g. %x.addr.0
        TypeInfo typeInfo;
        std::string llvmType;
    };

    struct StructDef {
        std::string name;
        std::vector<Parameter> fields;
        std::vector<ASTNodePtr> methods;
        ASTNodePtr constructor;
    };

    struct FuncInfo {
        std::string name;
        std::vector<Parameter> params;
        TypeInfo returnType;
        ASTNodePtr body;
        std::string structName; // non-empty for member methods
    };

    std::unordered_map<std::string, StructDef> structDefs;
    std::unordered_map<std::string, FuncInfo> funcInfos;

    // Variable scopes (stack)
    using Scope = std::unordered_map<std::string, VarInfo>;
    std::vector<Scope> scopes;
    std::string currentStructName; // for 'this' inside methods

    void pushScope();
    void popScope();
    VarInfo* findVar(const std::string& name);
    void declareVar(const std::string& name, VarInfo info);

    std::string newTemp();
    std::string newLabel();
    int newVarIndex();

    // Type helpers
    std::string llvmType(const TypeInfo& ti);
    std::string llvmType(const std::string& base);
    std::string llvmStructType(const std::string& structName);

    // Registration pass
    void collectDecls();
    void collectStruct(ASTNodePtr node);
    void collectFunction(ASTNodePtr node);

    // Emission
    void emitStructTypes();
    void emitFunctionDecls();
    void emitImport(ASTNodePtr node);
    void emitFunction(const FuncInfo& fi);
    void emitConstructor(const std::string& structName, ASTNodePtr node);
    void emitMemberMethod(const std::string& structName, ASTNodePtr node);

    // Statements → emit to `out`, return "" 
    void emitBlock(ASTNodePtr node);
    void emitStatement(ASTNodePtr node);
    void emitVarDecl(ASTNodePtr node);
    void emitReturn(ASTNodePtr node);
    void emitIf(ASTNodePtr node);
    void emitWhile(ASTNodePtr node);
    void emitFor(ASTNodePtr node);
    void emitExprStmt(ASTNodePtr node);

    // Expressions → return llvm value string (e.g. "%t0" or "42")
    std::string emitExpr(ASTNodePtr node);
    std::string emitBinaryOp(ASTNodePtr node);
    std::string emitAssign(ASTNodePtr node);
    std::string emitCompoundAssign(ASTNodePtr node);
    std::string emitCall(ASTNodePtr node);
    std::string emitMethodCall(ASTNodePtr node);
    std::string emitMemberAccess(ASTNodePtr node);
    std::string emitStructInit(const std::string& typeName, const std::vector<ASTNodePtr>& args);

    // Helper: get GEP for struct field
    std::string emitFieldGEP(const std::string& ptrReg, const std::string& structName, const std::string& fieldName, bool load = false);
    int fieldIndex(const std::string& structName, const std::string& fieldName);
};
