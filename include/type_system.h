#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

struct StructField {
    std::string name;
    TypeInfo type;
    int offset; // byte offset within struct
};

struct StructMethod {
    std::string name;
    std::vector<std::pair<std::string, TypeInfo>> params;
    TypeInfo returnType;
    ASTNode* body; // not owned
};

struct StructInfo {
    std::string name;
    std::vector<StructField> fields;
    std::vector<std::pair<std::string, TypeInfo>> constructorParams;
    ASTNode* constructorBody = nullptr; // not owned
    std::vector<StructMethod> methods;
    int totalSize = 0;
    
    int fieldOffset(const std::string& fieldName) const;
    int fieldIndex(const std::string& fieldName) const;
    TypeInfo fieldType(const std::string& fieldName) const;
    const StructMethod* findMethod(const std::string& methodName) const;
};

class TypeSystem {
public:
    void registerStruct(const std::string& name, StructInfo info);
    bool hasStruct(const std::string& name) const;
    const StructInfo& getStruct(const std::string& name) const;
    
    static int sizeOf(const TypeInfo& type);
    static bool isPrimitive(const std::string& typeName);
    static bool isVoid(const TypeInfo& type);

private:
    std::unordered_map<std::string, StructInfo> structs;
};
