#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <stdexcept>

struct LLVMValue {
    std::string reg;      // LLVM register name or constant
    std::string type;     // LLVM type string
    bool is_ptr = false;  // Is this a pointer to the actual value?
};

struct StructLayout {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields; // (field_name, llvm_type)
    std::unordered_map<std::string, int> field_index;
    // Methods stored as mangled function names
    std::unordered_map<std::string, ASTNodePtr> methods;
    ASTNodePtr constructor;
};

class LLVMCodegen {
public:
    LLVMCodegen();
    std::string generate(ASTNodePtr program);
    
private:
    std::ostringstream out;
    int reg_counter = 0;
    int label_counter = 0;
    
    // Variable -> (llvm_reg, llvm_type)
    std::vector<std::unordered_map<std::string, std::pair<std::string, std::string>>> var_scopes;
    
    std::unordered_map<std::string, StructLayout> struct_layouts;
    std::unordered_map<std::string, std::pair<std::vector<Parameter>, TypeInfo>> func_signatures;
    
    // Current function's struct context (for methods)
    std::string current_struct_type;
    std::string this_ptr_reg;
    
    std::string newReg();
    std::string newLabel();
    
    std::string typeToLLVM(const TypeInfo& t);
    std::string typeToLLVM(const std::string& base);
    std::string structTypeName(const std::string& name);
    
    void pushScope();
    void popScope();
    void declareVar(const std::string& name, const std::string& reg, const std::string& type);
    std::pair<std::string, std::string> lookupVar(const std::string& name);
    
    // Code generation methods
    void genProgram(ASTNodePtr node);
    void genStructDecl(ASTNodePtr node);
    void genFuncDecl(ASTNodePtr node);
    void genConstructor(ASTNodePtr node, const std::string& struct_name);
    void genMethodDecl(ASTNodePtr node, const std::string& struct_name);
    void genImport(ASTNodePtr node);
    
    LLVMValue genStmt(ASTNodePtr node);
    LLVMValue genExpr(ASTNodePtr node);
    LLVMValue genBlock(ASTNodePtr node);
    LLVMValue genVarDecl(ASTNodePtr node);
    LLVMValue genArrayDecl(ASTNodePtr node);
    LLVMValue genIfStmt(ASTNodePtr node);
    LLVMValue genWhileStmt(ASTNodePtr node);
    LLVMValue genForStmt(ASTNodePtr node);
    LLVMValue genReturnStmt(ASTNodePtr node);
    LLVMValue genBinaryOp(ASTNodePtr node);
    LLVMValue genAssign(ASTNodePtr node);
    LLVMValue genCompoundAssign(ASTNodePtr node);
    LLVMValue genFuncCall(ASTNodePtr node);
    LLVMValue genMethodCall(ASTNodePtr node);
    LLVMValue genMemberAccess(ASTNodePtr node);
    LLVMValue genIdentifier(ASTNodePtr node);
    LLVMValue genArrayAccess(ASTNodePtr node);
    LLVMValue genAddressOf(ASTNodePtr node);
    LLVMValue genDereference(ASTNodePtr node);
    
    // Helper: load value from pointer
    LLVMValue loadValue(const LLVMValue& ptr_val);
    // Helper: get GEP for struct field
    std::string getStructFieldPtr(const std::string& struct_ptr_reg, const std::string& struct_type, const std::string& field_name, std::string& field_llvm_type);
    
    // Mangled method name
    std::string mangleMethod(const std::string& struct_name, const std::string& method_name);
    std::string mangleConstructor(const std::string& struct_name);
    
    // Track variable types for struct instance tracking
    std::unordered_map<std::string, std::string> var_struct_type; // var_name -> struct_type_name
};
