#pragma once
#include "token.h"
#include "ast.h"
#include <vector>
#include <stdexcept>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    ASTNodePtr parse();

private:
    std::vector<Token> tokens;
    size_t pos;

    Token& current();
    Token& peek(int offset = 1);
    Token consume();
    Token expect(TokenType type);
    bool check(TokenType type) const;
    bool checkNext(TokenType type) const;
    void advance();

    ASTNodePtr parseProgram();
    ASTNodePtr parseTopLevel();
    ASTNodePtr parseImport();
    ASTNodePtr parseFuncDecl();
    ASTNodePtr parseStructDecl();
    ASTNodePtr parseVarDecl();
    
    // Struct members
    ASTNodePtr parseStructField();
    ASTNodePtr parseConstructorDecl();
    ASTNodePtr parseMethodDecl();
    
    // Statements
    ASTNodePtr parseBlock();
    ASTNodePtr parseStatement();
    ASTNodePtr parseReturnStmt();
    ASTNodePtr parseIfStmt();
    ASTNodePtr parseWhileStmt();
    ASTNodePtr parseForStmt();
    ASTNodePtr parseVarDeclStmt();
    ASTNodePtr parseExprStmt();
    
    // Expressions
    ASTNodePtr parseExpr();
    ASTNodePtr parseAssign();
    ASTNodePtr parseComparison();
    ASTNodePtr parseAddSub();
    ASTNodePtr parseMulDiv();
    ASTNodePtr parseUnary();
    ASTNodePtr parsePostfix();
    ASTNodePtr parsePrimary();
    
    // Type parsing
    TypeInfo parseType();
    std::vector<std::pair<std::string, TypeInfo>> parseParams();
    std::vector<ASTNodePtr> parseArgs();
    
    bool isTypeToken() const;
    bool isTypeToken(const Token& t) const;
};
