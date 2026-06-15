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

    Token current() const;
    Token peek(int offset = 1) const;
    Token consume();
    Token expect(TokenType type, const std::string& msg = "");
    bool check(TokenType type) const;
    bool match(TokenType type);

    // Top-level
    ASTNodePtr parseTopLevel();
    ASTNodePtr parseFunctionDecl();
    ASTNodePtr parseStructDecl();
    ASTNodePtr parseImportDecl();
    ASTNodePtr parseVarDecl();
    ASTNodePtr parseConstructorDecl(const std::string& structName);
    ASTNodePtr parseMemberMethodDecl(const std::string& structName);

    // Statements
    ASTNodePtr parseBlock();
    ASTNodePtr parseStatement();
    ASTNodePtr parseReturnStmt();
    ASTNodePtr parseIfStmt();
    ASTNodePtr parseWhileStmt();
    ASTNodePtr parseForStmt();
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

    // Types
    TypeInfo parseTypeInfo();
    std::vector<Parameter> parseParamList();
};
