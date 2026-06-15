#include "ast.h"

ASTNodePtr makeNode(ASTNodeType type) {
    auto node = std::make_shared<ASTNode>();
    node->type = type;
    return node;
}

ASTNodePtr makeIntLiteral(int value) {
    auto node = makeNode(ASTNodeType::IntLiteral);
    node->intValue = value;
    return node;
}

ASTNodePtr makeFloatLiteral(double value) {
    auto node = makeNode(ASTNodeType::FloatLiteral);
    node->floatValue = value;
    return node;
}

ASTNodePtr makeStringLiteral(const std::string& value) {
    auto node = makeNode(ASTNodeType::StringLiteral);
    node->strValue = value;
    return node;
}

ASTNodePtr makeIdentifier(const std::string& name) {
    auto node = makeNode(ASTNodeType::Identifier);
    node->name = name;
    return node;
}
