#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include "type_system.h"

enum class OpCode {
    // Stack ops
    PUSH_INT,
    PUSH_FLOAT,
    POP,
    DUP,
    
    // Arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    
    // Float arithmetic
    FADD,
    FSUB,
    FMUL,
    FDIV,
    
    // Comparison
    EQ,
    NEQ,
    LT,
    LE,
    GT,
    GE,
    
    // Memory
    LOAD,       // load local variable (index)
    STORE,      // store local variable (index)
    LOAD_GLOBAL,
    STORE_GLOBAL,
    LOAD_FIELD,   // load field from struct on stack
    STORE_FIELD,  // store field into struct on stack
    ADDR_OF,      // push address of local
    DEREF,        // dereference pointer
    STORE_DEREF,  // store through pointer
    LOAD_ARRAY,   // array index load
    STORE_ARRAY,  // array index store
    ALLOC_STRUCT, // allocate struct, push pointer
    
    // Control flow
    JMP,
    JMP_IF_FALSE,
    CALL,
    CALL_BUILTIN,
    RETURN,
    RETURN_VOID,
    
    // Method call
    CALL_METHOD,
    
    // Misc
    NOP,
};

struct Instruction {
    OpCode op;
    long long iOperand = 0;
    double fOperand = 0.0;
    std::string sOperand;
};

struct CallFrame {
    std::vector<long long> locals;
    size_t returnAddr;
    int returnLocal; // where to store return value (-1 if void or top-of-stack)
};

class VM {
public:
    explicit VM(const TypeSystem& ts);
    
    void addFunction(const std::string& name, std::vector<Instruction> code);
    void addBuiltin(const std::string& name, std::function<long long(std::vector<long long>)> fn);
    
    long long run(const std::string& entryPoint);

private:
    const TypeSystem& typeSystem;
    std::unordered_map<std::string, std::vector<Instruction>> functions;
    std::unordered_map<std::string, std::function<long long(std::vector<long long>)>> builtins;
    
    std::vector<long long> stack;
    std::vector<CallFrame> callStack;
    std::vector<long long> heap; // simple heap for structs
    
    long long pop();
    void push(long long v);
    long long allocHeap(int size);
    
    void executeFunction(const std::string& name);
};
