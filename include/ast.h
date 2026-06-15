#pragma once
#include <string>
#include <vector>
#include <memory>

// Forward declarations
struct ASTNode;
using ASTNodePtr = std::shared_ptr<ASTNode>;

enum class ASTNodeType {
    // Program
    Program,

    // Declarations
    FunctionDecl,
    StructDecl,
    ImportDecl,
    VarDecl,
    ConstructorDecl,
    MemberMethodDecl,

    // Statements
    Block,
    ReturnStmt,
    IfStmt,
    WhileStmt,
    ForStmt,
    ExprStmt,

    // Expressions
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    Identifier,
    BinaryOp,
    AssignOp,
    CompoundAssignOp,
    CallExpr,
    IndexExpr,
    MemberAccess,
    MethodCall,
    AddressOf,
    Deref,
    This,
    StructInit,
    ArrayInit,
};

struct TypeInfo {
    std::string baseType;   // "int", "char", "long", "float", "double", "void", or struct name
    bool isPointer = false;
    bool isArray = false;
    int arraySize = 0;
};

struct Parameter {
    TypeInfo type;
    std::string name;
};

struct ASTNode {
    ASTNodeType type;

    // For literals
    int intValue = 0;
    double floatValue = 0.0;
    std::string strValue;

    // For declarations
    std::string name;
    TypeInfo typeInfo;
    TypeInfo returnType;

    // Children
    std::vector<ASTNodePtr> children;
    std::vector<Parameter> params;

    // For binary ops
    std::string op;

    // For if/while/for
    ASTNodePtr condition;
    ASTNodePtr thenBranch;
    ASTNodePtr elseBranch;
    ASTNodePtr init;
    ASTNodePtr update;

    // For function/method body
    ASTNodePtr body;

    // For call expressions
    ASTNodePtr callee;
    std::vector<ASTNodePtr> args;

    // For member access / method call
    ASTNodePtr object;
    std::string memberName;

    // For struct declarations
    std::vector<Parameter> fields;
    std::vector<ASTNodePtr> methods;      // MemberMethodDecl nodes
    ASTNodePtr constructor;               // ConstructorDecl node

    // For array init
    std::vector<ASTNodePtr> elements;
};

// Helper factory functions
ASTNodePtr makeNode(ASTNodeType type);
ASTNodePtr makeIntLiteral(int value);
ASTNodePtr makeFloatLiteral(double value);
ASTNodePtr makeStringLiteral(const std::string& value);
ASTNodePtr makeIdentifier(const std::string& name);
