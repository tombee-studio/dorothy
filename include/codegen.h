#pragma once
#include "ast.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <stdexcept>

// Value type for the VM
struct Value {
    enum class Type { INT, FLOAT, POINTER } type;
    union {
        int int_val;
        double float_val;
        int ptr_val;
    };
    
    Value() : type(Type::INT), int_val(0) {}
    Value(int v) : type(Type::INT), int_val(v) {}
    Value(double v) : type(Type::FLOAT), float_val(v) {}
    
    int asInt() const {
        if (type == Type::INT || type == Type::POINTER) return int_val;
        return (int)float_val;
    }
    
    double asFloat() const {
        if (type == Type::FLOAT) return float_val;
        return (double)int_val;
    }
};

struct StructTypeInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeInfo>> fields;
    // Methods: name -> (params, body)
    std::unordered_map<std::string, ASTNodePtr> methods;
    ASTNodePtr constructor;
};

class Environment {
public:
    Environment(Environment* parent = nullptr) : parent(parent) {}
    
    void set(const std::string& name, Value val) {
        vars[name] = val;
    }
    
    Value& get(const std::string& name) {
        if (vars.count(name)) return vars[name];
        if (parent) return parent->get(name);
        throw std::runtime_error("Undefined variable: " + name);
    }
    
    bool has(const std::string& name) const {
        if (vars.count(name)) return true;
        if (parent) return parent->has(name);
        return false;
    }
    
    Environment* parent;
    std::unordered_map<std::string, Value> vars;
};

class Interpreter {
public:
    Interpreter();
    int run(ASTNodePtr program);
    
private:
    std::unordered_map<std::string, ASTNodePtr> functions;
    std::unordered_map<std::string, StructTypeInfo> struct_types;
    // Heap for struct instances
    std::vector<std::unordered_map<std::string, Value>> heap;
    
    struct ReturnException {
        Value value;
    };
    
    Value eval(ASTNodePtr node, Environment& env);
    Value evalBlock(ASTNodePtr node, Environment& env);
    Value callFunction(const std::string& name, const std::vector<Value>& args);
    Value callMethod(int obj_ptr, const std::string& struct_type, const std::string& method, const std::vector<Value>& args);
    
    void registerBuiltins();
    std::unordered_map<std::string, std::function<Value(std::vector<Value>)>> builtins;
    
    // Struct instance stores: heap index -> (type_name, fields)
    std::vector<std::string> heap_types;
    
    int allocStruct(const std::string& type_name);
    std::unordered_map<std::string, Value>& getStructInstance(int ptr);
    std::string getStructType(int ptr);
};
