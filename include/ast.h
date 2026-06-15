#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>

// Forward declarations
struct ASTNode;
using ASTNodePtr = std::shared_ptr<ASTNode>;

enum class ASTNodeType {
    // Literals
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    
    // Identifiers
    IDENTIFIER,
    THIS_EXPR,
    
    // Binary operations
    BINARY_OP,
    ASSIGN,
    COMPOUND_ASSIGN,
    
    // Unary operations
    ADDRESS_OF,
    DEREFERENCE,
    
    // Control flow
    IF_STMT,
    WHILE_STMT,
    FOR_STMT,
    RETURN_STMT,
    
    // Declarations
    VAR_DECL,
    ARRAY_DECL,
    FUNC_DECL,
    IMPORT_DECL,
    STRUCT_DECL,
    CONSTRUCTOR_DECL,
    METHOD_DECL,
    
    // Expressions
    FUNC_CALL,
    METHOD_CALL,
    MEMBER_ACCESS,
    ARRAY_ACCESS,
    
    // Statements
    EXPR_STMT,
    BLOCK,
    
    // Program
    PROGRAM
};

struct TypeInfo {
    std::string base_type;  // int, char, long, float, double, void, or struct name
    bool is_pointer = false;
    int array_size = 0;     // 0 = not array
    
    TypeInfo() : base_type("int") {}
    TypeInfo(const std::string& t) : base_type(t) {}
    TypeInfo(const std::string& t, bool ptr) : base_type(t), is_pointer(ptr) {}
};

struct Parameter {
    TypeInfo type;
    std::string name;
};

struct ASTNode {
    ASTNodeType node_type;
    
    // For literals
    int int_value = 0;
    double float_value = 0.0;
    std::string string_value;
    
    // For typed nodes
    TypeInfo type_info;
    
    // For binary ops
    std::string op;
    
    // Children
    std::vector<ASTNodePtr> children;
    
    // For function/method declarations
    std::string name;
    std::vector<Parameter> params;
    TypeInfo return_type;
    
    // For struct declarations
    std::vector<ASTNodePtr> members;      // var declarations
    std::vector<ASTNodePtr> methods;      // method declarations
    ASTNodePtr constructor_node;
    
    // For member access / method calls
    ASTNodePtr object;
    std::string member_name;
    std::vector<ASTNodePtr> args;
    
    // For array
    std::vector<ASTNodePtr> init_values;
    
    ASTNode(ASTNodeType t) : node_type(t) {}
};

// Helper constructors
ASTNodePtr makeIntLiteral(int value);
ASTNodePtr makeFloatLiteral(double value);
ASTNodePtr makeStringLiteral(const std::string& value);
ASTNodePtr makeIdentifier(const std::string& name);
ASTNodePtr makeThisExpr();
ASTNodePtr makeBinaryOp(const std::string& op, ASTNodePtr left, ASTNodePtr right);
ASTNodePtr makeAssign(ASTNodePtr target, ASTNodePtr value);
ASTNodePtr makeCompoundAssign(const std::string& op, ASTNodePtr target, ASTNodePtr value);
ASTNodePtr makeAddressOf(ASTNodePtr expr);
ASTNodePtr makeDereference(ASTNodePtr expr);
ASTNodePtr makeIfStmt(ASTNodePtr cond, ASTNodePtr then_block, ASTNodePtr else_block);
ASTNodePtr makeWhileStmt(ASTNodePtr cond, ASTNodePtr body);
ASTNodePtr makeForStmt(ASTNodePtr init, ASTNodePtr cond, ASTNodePtr update, ASTNodePtr body);
ASTNodePtr makeReturnStmt(ASTNodePtr value);
ASTNodePtr makeVarDecl(const std::string& name, const TypeInfo& type, ASTNodePtr init = nullptr);
ASTNodePtr makeArrayDecl(const std::string& name, const TypeInfo& type, int size, std::vector<ASTNodePtr> inits);
ASTNodePtr makeFuncDecl(const std::string& name, const std::vector<Parameter>& params, const TypeInfo& return_type, ASTNodePtr body);
ASTNodePtr makeConstructorDecl(const std::string& struct_name, const std::vector<Parameter>& params, ASTNodePtr body);
ASTNodePtr makeMethodDecl(const std::string& name, const std::vector<Parameter>& params, const TypeInfo& return_type, ASTNodePtr body);
ASTNodePtr makeImportDecl(const std::string& name);
ASTNodePtr makeStructDecl(const std::string& name, std::vector<ASTNodePtr> members, std::vector<ASTNodePtr> methods, ASTNodePtr constructor);
ASTNodePtr makeFuncCall(const std::string& name, std::vector<ASTNodePtr> args);
ASTNodePtr makeMethodCall(ASTNodePtr object, const std::string& method_name, std::vector<ASTNodePtr> args);
ASTNodePtr makeMemberAccess(ASTNodePtr object, const std::string& member_name);
ASTNodePtr makeArrayAccess(ASTNodePtr array, ASTNodePtr index);
ASTNodePtr makeExprStmt(ASTNodePtr expr);
ASTNodePtr makeBlock(std::vector<ASTNodePtr> stmts);
ASTNodePtr makeProgram(std::vector<ASTNodePtr> decls);
