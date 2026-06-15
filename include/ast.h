#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>

// Forward declarations
struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

enum class ASTNodeType {
    // Program
    PROGRAM,
    
    // Declarations
    FUNC_DECL,
    STRUCT_DECL,
    VAR_DECL,
    IMPORT_DECL,
    PARAM,
    
    // Struct members
    STRUCT_FIELD,
    STRUCT_CONSTRUCTOR,
    STRUCT_METHOD,
    
    // Statements
    BLOCK,
    RETURN_STMT,
    IF_STMT,
    WHILE_STMT,
    FOR_STMT,
    EXPR_STMT,
    
    // Expressions
    ASSIGN_EXPR,
    BINARY_EXPR,
    UNARY_EXPR,
    CALL_EXPR,
    METHOD_CALL_EXPR,
    MEMBER_ACCESS_EXPR,
    INDEX_EXPR,
    IDENTIFIER_EXPR,
    THIS_EXPR,
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    ARRAY_LITERAL,
};

struct TypeInfo {
    std::string base;       // int, char, long, float, double, void, or struct name
    bool isPointer = false;
    int arraySize = 0;      // 0 means not an array
};

struct ASTNode {
    ASTNodeType type;
    
    // Common fields
    std::string name;
    std::string op;
    TypeInfo typeInfo;
    
    // Value for literals
    std::variant<long long, double, std::string> value;
    
    // Children
    std::vector<ASTNodePtr> children;
    
    // For functions/methods
    std::vector<std::pair<std::string, TypeInfo>> params;  // (name, type)
    TypeInfo returnType;
    
    // Line info
    int line = 0;
    int col = 0;
};

// Helper to create nodes
ASTNodePtr makeNode(ASTNodeType type);
