#include "ast.h"

ASTNodePtr makeNode(ASTNodeType type) {
    auto node = std::make_unique<ASTNode>();
    node->type = type;
    return node;
}
