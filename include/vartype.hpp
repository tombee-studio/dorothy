/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once
#include <string>
#include <vector>

enum class VarType {
    CHAR,     // i8  - 1 byte integer
    INT,      // i32 - 4 byte integer
    LONG,     // i64 - 8 byte integer
    FLOAT,    // float  - 4 byte floating point
    DOUBLE,   // double - 8 byte floating point
    STRING,   // variable-length string (ptr)
    STRUCT,   // user-defined struct type
    CLASS,    // user-defined class type (reference semantics, pointer)
    ARRAY,    // dynamic array (Array<T> / T[], pointer)
    INFERRED, // placeholder for type inference; resolved in llvm_emit
};

struct TypeInfo {
    VarType base_type = VarType::LONG;
    std::string type_name;          // e.g. "Player", "Box", "Array"
    bool is_nullable = false;
    std::vector<TypeInfo> generic_args; // e.g. for Array<T>, [TypeInfo for T]

    bool is_array() const {
        return base_type == VarType::ARRAY || type_name == "Array";
    }

    TypeInfo get_element_type() const {
        if (!generic_args.empty()) return generic_args[0];
        return TypeInfo{VarType::LONG, "", false, {}};
    }
};

inline std::string llvm_type_str(VarType t) {
    switch (t) {
        case VarType::CHAR:     return "i8";
        case VarType::INT:      return "i32";
        case VarType::LONG:     return "i64";
        case VarType::FLOAT:    return "float";
        case VarType::DOUBLE:   return "double";
        case VarType::STRING:   return "ptr";
        case VarType::STRUCT:   return "ptr";
        case VarType::CLASS:    return "ptr";
        case VarType::ARRAY:    return "ptr";
        case VarType::INFERRED: return "i64";
    }
    return "i64";
}

inline bool is_float_type(VarType t) {
    return t == VarType::FLOAT || t == VarType::DOUBLE;
}

inline bool is_string_type(VarType t) {
    return t == VarType::STRING;
}

// Returns the canonical computation type (STRING for string, double for floats, i64 for integers)
inline VarType canonical_type(VarType t) {
    if (is_string_type(t)) return VarType::STRING;
    return is_float_type(t) ? VarType::DOUBLE : VarType::LONG;
}

// Promote two canonical types (STRING vs DOUBLE vs LONG)
inline VarType promote_canonical(VarType a, VarType b) {
    if (a == VarType::STRING || b == VarType::STRING) return VarType::STRING;
    if (a == VarType::DOUBLE || b == VarType::DOUBLE) return VarType::DOUBLE;
    return VarType::LONG;
}
