/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#pragma once
#include <string>

enum class VarType {
    CHAR,     // i8  - 1 byte integer
    INT,      // i32 - 4 byte integer
    LONG,     // i64 - 8 byte integer
    FLOAT,    // float  - 4 byte floating point
    DOUBLE,   // double - 8 byte floating point
    STRUCT,   // user-defined struct type
    INFERRED, // placeholder for type inference; resolved in llvm_emit
};

inline std::string llvm_type_str(VarType t) {
    switch (t) {
        case VarType::CHAR:     return "i8";
        case VarType::INT:      return "i32";
        case VarType::LONG:     return "i64";
        case VarType::FLOAT:    return "float";
        case VarType::DOUBLE:   return "double";
        case VarType::STRUCT:   return "ptr";
        case VarType::INFERRED: return "i64";
    }
    return "i64";
}

inline bool is_float_type(VarType t) {
    return t == VarType::FLOAT || t == VarType::DOUBLE;
}

// Returns the canonical computation type (i64 for integers, double for floats)
inline VarType canonical_type(VarType t) {
    return is_float_type(t) ? VarType::DOUBLE : VarType::LONG;
}

// Promote two canonical types (LONG vs DOUBLE)
inline VarType promote_canonical(VarType a, VarType b) {
    if (a == VarType::DOUBLE || b == VarType::DOUBLE) return VarType::DOUBLE;
    return VarType::LONG;
}
