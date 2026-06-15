#pragma once
#include "ast.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>
#include <functional>
#include <variant>

// Value types for the interpreter
struct StructValue;
using Value = std::variant<int, long long, double, std::shared_ptr<StructValue>>;

struct StructValue {
    std::string typeName;
    std::unordered_map<std::string, Value> fields;
};

struct ReturnException {
    Value value;
};

class Interpreter {
public:
    explicit Interpreter(ASTNodePtr ast);
    int run();

private:
    ASTNodePtr ast;

    struct StructDef {
        std::string name;
        std::vector<Parameter> fields;
        ASTNodePtr constructor;
        std::vector<ASTNodePtr> methods;
    };

    struct FuncDef {
        std::string name;
        std::vector<Parameter> params;
        ASTNodePtr body;
        TypeInfo returnType;
    };

    std::unordered_map<std::string, StructDef> structDefs;
    std::unordered_map<std::string, FuncDef> funcDefs;

    // Variable scopes (stack of frames)
    using Env = std::unordered_map<std::string, Value>;
    std::vector<Env> envStack;

    void pushEnv();
    void popEnv();
    Value& lookupVar(const std::string& name);
    void declareVar(const std::string& name, Value val);
    void setVar(const std::string& name, Value val);

    // This pointer for member methods
    std::shared_ptr<StructValue> currentThis;

    // Registration
    void registerDecls();
    void registerStruct(ASTNodePtr node);
    void registerFunction(ASTNodePtr node);

    // Execution
    Value execBlock(ASTNodePtr node);
    Value execStatement(ASTNodePtr node);
    Value execVarDecl(ASTNodePtr node);
    Value execReturn(ASTNodePtr node);
    Value execIf(ASTNodePtr node);
    Value execWhile(ASTNodePtr node);
    Value execFor(ASTNodePtr node);
    Value execExprStmt(ASTNodePtr node);

    // Expressions
    Value evalExpr(ASTNodePtr node);
    Value evalBinaryOp(ASTNodePtr node);
    Value evalAssign(ASTNodePtr node);
    Value evalCompoundAssign(ASTNodePtr node);
    Value evalCall(ASTNodePtr node);
    Value evalMethodCall(ASTNodePtr node);
    Value evalMemberAccess(ASTNodePtr node);
    Value evalStructInit(const std::string& typeName, const std::vector<ASTNodePtr>& args);

    // Built-in functions
    std::unordered_map<std::string, std::function<Value(std::vector<Value>)>> builtins;
    void registerBuiltins();

    // Helpers
    Value defaultValue(const TypeInfo& ti);
    int toInt(const Value& v);
    long long toLong(const Value& v);
    double toDouble(const Value& v);
    Value numericBinaryOp(const std::string& op, const Value& lhs, const Value& rhs);
    bool isTruthy(const Value& v);
};
