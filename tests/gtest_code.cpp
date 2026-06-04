/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include <gtest/gtest.h>

#include <sstream>

#include "../include/code.hpp"

TEST(CodeTest, MakeCodeWithMnemonicADD) {
    auto code = Code::makeCode(Code::ADD, 1, 2);
    EXPECT_EQ(code.mnemonic, Code::ADD);
    EXPECT_EQ(code.op1, 1);
    EXPECT_EQ(code.op2, 2);
}

TEST(CodeTest, MakeCodeWithMnemonicPUSHI) {
    auto code = Code::makeCode(Code::PUSHI, 42, 0);
    EXPECT_EQ(code.mnemonic, Code::PUSHI);
    EXPECT_EQ(code.op1, 42);
    EXPECT_EQ(code.op2, 0);
}

TEST(CodeTest, MakeCodeWithMnemonicCALL) {
    auto code = Code::makeCode(Code::CALL, 5, 0);
    EXPECT_EQ(code.mnemonic, Code::CALL);
    EXPECT_EQ(code.op1, 5);
    EXPECT_EQ(code.op2, 0);
}

TEST(CodeTest, MakeCodeWithMnemonicJMP) {
    auto code = Code::makeCode(Code::JMP, 10, 0);
    EXPECT_EQ(code.mnemonic, Code::JMP);
    EXPECT_EQ(code.op1, 10);
}

TEST(CodeTest, MakeCodeFromStringADD) {
    auto code = Code::makeCode("     ADD     1     2");
    EXPECT_EQ(code.mnemonic, Code::ADD);
    EXPECT_EQ(code.op1, 1);
    EXPECT_EQ(code.op2, 2);
}

TEST(CodeTest, MakeCodeFromStringPUSHI) {
    auto code = Code::makeCode("   PUSHI    42     0");
    EXPECT_EQ(code.mnemonic, Code::PUSHI);
    EXPECT_EQ(code.op1, 42);
    EXPECT_EQ(code.op2, 0);
}

TEST(CodeTest, GetCodeFromNameRoundtrip) {
    const std::vector<std::pair<std::string, Code::Mnemonic>> cases = {
        {"PUSHI", Code::PUSHI}, {"PUSHR", Code::PUSHR}, {"POP", Code::POP},
        {"ADD", Code::ADD},     {"SUB", Code::SUB},     {"MUL", Code::MUL},
        {"DIV", Code::DIV},     {"MOD", Code::MOD},     {"EQ", Code::EQ},
        {"NE", Code::NE},       {"LT", Code::LT},       {"LE", Code::LE},
        {"GT", Code::GT},       {"GE", Code::GE},       {"JMP", Code::JMP},
        {"JE", Code::JE},       {"JNE", Code::JNE},     {"STORE", Code::STORE},
        {"LOAD", Code::LOAD},   {"CALL", Code::CALL},   {"RET", Code::RET},
        {"EXIT", Code::EXIT},   {"MOVE", Code::MOVE},   {"MOVEI", Code::MOVEI},
        {"INT", Code::INT},
    };
    for (auto& [name, mnemonic] : cases) {
        EXPECT_EQ(Code::getCodeFromName(name), mnemonic) << "failed for: " << name;
    }
}

TEST(CodeTest, PrintContainsMnemonicName) {
    auto code = Code::makeCode(Code::ADD, 10, 20);
    std::ostringstream oss;
    code.print(oss);
    EXPECT_NE(oss.str().find("ADD"), std::string::npos);
}

TEST(CodeTest, PrintContainsOperands) {
    auto code = Code::makeCode(Code::PUSHI, 99, 0);
    std::ostringstream oss;
    code.print(oss);
    EXPECT_NE(oss.str().find("99"), std::string::npos);
}
