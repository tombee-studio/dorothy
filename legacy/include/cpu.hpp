/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#ifndef CPU_HPP_
#define CPU_HPP_

#include <iostream>
#include <unordered_set>
#include <vector>

#include "./code.hpp"
using std::vector;
using std::unordered_set;
using std::ostream;
using std::endl;

#define REG_SIZE 4
#ifndef STACK_SIZE
#define STACK_SIZE 262144
#endif
#define BP_INDEX 0
#define SP_INDEX 1

class CPU {
    bool _exit;
    int pc;
    int reg[REG_SIZE];
    int _memory[STACK_SIZE];
    vector<int (*)(int, int *)> funcs;

    vector<Code> _codes;
    unordered_set<int> _allocated;

 public:
    CPU() : pc(0), _exit(false) {
        reg[0] = STACK_SIZE - 1;
        reg[1] = STACK_SIZE - 1;
        reg[2] = 0;
        reg[3] = 0;
        for (int i = 0; i < STACK_SIZE; i++) {
            _memory[i] = 0;
        }
    }

    void set(vector<Code> &);
    void run();
    int exe();
    void print(ostream &);
    void add(int (*func)(int, int *));

 private:
    void run(Code);
    int allocate(int size);
};

class CantAccessToBadFieldException : public std::runtime_error {
    CPU _cpu;

 public:
    CantAccessToBadFieldException(const char *_Message, CPU cpu)
        : runtime_error(_Message), _cpu(cpu) {}
};

#endif  // CPU_HPP_
