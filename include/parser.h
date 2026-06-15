#pragma once
#include "token.h"
#include "ast.h"
#include <vector>
#include <stdexcept>

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    ASTNodePtr parse();
    
private:
    std::vector<Token> tokens;
    size_t pos;
    
    Token& current();
    Token& peek(int offset = 1);
    Token consume();
    Token expect(TokenType type);
    bool check(TokenType type);
    bool match(TokenType type);
    
    // Top-level
    ASTNodePtr parseProgram();
    ASTNodePtr parseTopLevel();
    ASTNodePtr parseFuncDecl();
    ASTNodePtr parseImportDecl();
    ASTNodePtr parseStructDecl();
    
    // Struct internals
    ASTNodePtr parseConstructorDecl(const std::string& struct_name);
    ASTNodePtr parseMethodDecl();
    
    // Statements
    ASTNodePtr parseStatement();
    ASTNodePtr parseVarDecl();
    ASTNodePtr parseIfStmt();
    ASTNodePtr parseWhileStmt();
    ASTNodePtr parseForStmt();
    ASTNodePtr parseReturnStmt();
    ASTNodePtr parseBlock();
    ASTNodePtr parseExprStmt();
    
    // Expressions
    ASTNodePtr parseExpression();
    ASTNodePtr parseAssignment();
    ASTNodePtr parseComparison();
    ASTNodePtr parseAddSub();
    ASTNodePtr parseMulDiv();
    ASTNodePtr parseUnary();
    ASTNodePtr parsePostfix();
    ASTNodePtr parsePrimary();
    
    // Type parsing
    TypeInfo parseType();
    std::vector<Parameter> parseParams();
    std::vector<ASTNodePtr> parseArgs();
    
    bool isTypeToken();
    bool isTypeToken(const Token& t);
};
