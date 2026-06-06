/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/parser.hpp"

vector<Function *> Parser::parse(vector<Token> &tokens) {
    _pos = 0;
    while (consume(tokens, Token::TK_EOF).type == Token::NONE) {
        _functions.push_back(parse_function(tokens));
    }
    return _functions;
}

Function *Parser::parse_function(vector<Token> &tokens) {
    if (consume(tokens, Token::KW_FUNC).type == Token::NONE) {
        if (consume(tokens, Token::KW_IMPORT).type == Token::NONE) {
            throw ParseError(format("position: %d", _pos), tokens[_pos]);
        } else {
            Token id_token = consume(tokens, Token::TK_ID);
            vector<DeclVar *> args;
            if (id_token.type == Token::NONE)
                throw ParseError("expected ID", tokens[_pos]);
            if (consume(tokens, (Token::Type)';').type != Token::NONE) {
                return new ImportFunction(id_token.id, args);
            } else {
                throw ParseError("expected ';'", tokens[_pos]);
            }
        }
    }
    Token id_token = consume(tokens, Token::TK_ID);
    if (id_token.type == Token::NONE)
        throw ParseError(format("position: %d", _pos), tokens[_pos]);
    auto declargs = parse_declargs(tokens);
    auto block = parse_block(tokens);
    return new Function(id_token.id, declargs, block);
}

static bool is_type_keyword(Token::Type t) {
    return t == Token::KW_INT || t == Token::KW_CHAR || t == Token::KW_LONG ||
           t == Token::KW_FLOAT || t == Token::KW_DOUBLE;
}

static VarType token_to_vartype(Token::Type t) {
    switch (t) {
        case Token::KW_CHAR:   return VarType::CHAR;
        case Token::KW_INT:    return VarType::INT;
        case Token::KW_LONG:   return VarType::LONG;
        case Token::KW_FLOAT:  return VarType::FLOAT;
        case Token::KW_DOUBLE: return VarType::DOUBLE;
        default:               return VarType::LONG;
    }
}

vector<DeclVar *> Parser::parse_declargs(vector<Token> &tokens) {
    vector<DeclVar *> declargs;
    if (consume(tokens, (Token::Type)'(').type == Token::NONE)
        throw ParseError(format("position: %d", _pos), tokens[_pos]);
    if (is_type_keyword(tokens[_pos].type)) {
        declargs.push_back(parse_declvar(tokens));
        while (consume(tokens, (Token::Type)',').type == (Token::Type)',') {
            declargs.push_back(parse_declvar(tokens));
        }
    }
    if (consume(tokens, (Token::Type)')').type == Token::NONE)
        throw ParseError(format("position: %d", _pos), tokens[_pos]);
    return declargs;
}

DeclVar *Parser::parse_declvar(vector<Token> &tokens) {
    if (!is_type_keyword(tokens[_pos].type)) return NULL;
    VarType vtype = token_to_vartype(tokens[_pos].type);
    _pos++;
    Token id_token = consume(tokens, Token::TK_ID);
    if (id_token.type == Token::NONE)
        throw ParseError(format("position: %d", _pos), tokens[_pos]);
    if (consume(tokens, (Token::Type)'[').type == Token::NONE) {
        return new DeclVar(id_token.id, vtype);
    } else {
        Token num_token;
        if ((num_token = consume(tokens, Token::TK_INT)).type != Token::NONE) {
            if (consume(tokens, (Token::Type)']').type != Token::NONE) {
                if (consume(tokens, (Token::Type)'=').type != Token::NONE) {
                    return new InitializedDeclArrayVar(
                        id_token.id, num_token.int_val,
                        parse_array_initializer(tokens), vtype);
                } else {
                    return new DeclArrayVar(id_token.id, num_token.int_val, vtype);
                }
            } else {
                throw ParseError("expected ']'", tokens[_pos]);
            }
        } else {
            throw ParseError("array variable should be initialized with 'INT'",
                             tokens[_pos]);
        }
    }
}

vector<Expression *> Parser::parse_array_initializer(vector<Token> &tokens) {
    if (consume(tokens, (Token::Type)'{').type == Token::NONE) {
        throw ParseError("expected '{'", tokens[_pos]);
    }
    vector<Expression *> exprs;
    auto exp = parse_expression(tokens);
    if (exp) {
        exprs.push_back(exp);
        while (consume(tokens, (Token::Type)',').type != Token::NONE) {
            exp = parse_expression(tokens);
            if (!exp)
                throw ParseError(format("expected 'expression' at %d", _pos),
                                 tokens[_pos]);
            exprs.push_back(exp);
        }
    }
    if (consume(tokens, (Token::Type)'}').type == Token::NONE) {
        throw ParseError("expected '}'", tokens[_pos]);
    }
    return exprs;
}

Block *Parser::parse_block(vector<Token> &tokens) {
    vector<Statement *> statements;
    if (consume(tokens, (Token::Type)'{').type == Token::NONE) return NULL;
    while (tokens[_pos].type != (Token::Type)'}') {
        auto st = parse_statement(tokens);
        if (st) {
            statements.push_back(st);
        } else {
            throw ParseError(format("position: %d", _pos), tokens[_pos]);
        }
    }
    if (consume(tokens, (Token::Type)'}').type == Token::NONE)
        throw ParseError(format("position: %d", _pos), tokens[_pos]);
    return new Block(statements);
}

Statement *Parser::parse_declvarst(vector<Token> &tokens) {
    auto decl = parse_declvar(tokens);
    if (!decl) {
        return NULL;
    } else {
        if (consume(tokens, (Token::Type)';').type == Token::NONE)
            throw ParseError(format("expected ';' at %d", _pos), tokens[_pos]);
        return new DeclVarSt(decl);
    }
}

Statement *Parser::parse_statement(vector<Token> &tokens) {
    Statement *statement;
    Expression *exp;
    if ((statement = parse_declvarst(tokens))) return statement;
    if ((statement = parse_block(tokens))) return statement;
    if ((statement = parse_ifst(tokens))) return statement;
    if ((statement = parse_forst(tokens))) return statement;
    if ((statement = parse_whilest(tokens))) return statement;
    if ((statement = parse_returnst(tokens))) return statement;
    if ((exp = parse_expression(tokens))) {
        if (consume(tokens, (Token::Type)';').type != Token::NONE) {
            return new ExpressionSt(exp);
        } else {
            throw ParseError("expected ';'", tokens[_pos]);
        }
    }
    return NULL;
}

Statement *Parser::parse_returnst(vector<Token> &tokens) {
    if (consume(tokens, Token::KW_RETURN).type == Token::NONE) return NULL;
    Expression *exp = parse_expression(tokens);
    if (!exp)
        throw ParseError(format("exprected 'expression' at %d", _pos),
                         tokens[_pos]);
    if (consume(tokens, (Token::Type)';').type == Token::NONE)
        throw ParseError(format("expected ';'"), tokens[_pos]);
    return new ReturnSt(exp);
}

Statement *Parser::parse_ifst(vector<Token> &tokens) {
    Statement *true_statement = NULL;
    Statement *else_statement = NULL;
    if (consume(tokens, Token::KW_IF).type == Token::NONE) return NULL;
    if (consume(tokens, (Token::Type)'(').type == Token::NONE)
        throw ParseError(format("expected '(' at %c", _pos), tokens[_pos]);
    auto exp = parse_expression(tokens);
    if (!exp)
        throw ParseError(format("exprected 'expression' at %d", _pos),
                         tokens[_pos]);
    if (consume(tokens, (Token::Type)')').type == Token::NONE)
        throw ParseError(format("expected ')' at %d", _pos), tokens[_pos]);
    true_statement = parse_statement(tokens);
    if (!true_statement)
        throw ParseError(format("exprected 'statement' at %d", _pos),
                         tokens[_pos]);
    if (consume(tokens, Token::KW_ELSE).type != Token::NONE) {
        else_statement = parse_statement(tokens);
    }
    return new IfSt(exp, true_statement, else_statement);
}

Statement *Parser::parse_forst(vector<Token> &tokens) {
    if (consume(tokens, Token::KW_FOR).type == Token::NONE) return NULL;
    if (consume(tokens, (Token::Type)'(').type == Token::NONE)
        throw ParseError(format("expected '(' at %c", _pos), tokens[_pos]);
    auto init = parse_expression(tokens);
    if (consume(tokens, (Token::Type)';').type == Token::NONE)
        throw ParseError(format("expected ';' at %d", _pos), tokens[_pos]);
    auto cond = parse_expression(tokens);
    if (consume(tokens, (Token::Type)';').type == Token::NONE)
        throw ParseError(format("expected ';' at %d", _pos), tokens[_pos]);
    auto proceed = parse_expression(tokens);
    if (!init || !proceed)
        throw ParseError(format("exprected 'expression' at %d", _pos),
                         tokens[_pos]);
    if (consume(tokens, (Token::Type)')').type == Token::NONE)
        throw ParseError(format("expected ')' at %d", _pos), tokens[_pos]);
    Statement *body = parse_statement(tokens);
    return new ForSt(init, cond, proceed, body);
}

Statement *Parser::parse_whilest(vector<Token> &tokens) {
    if (consume(tokens, Token::KW_WHILE).type == Token::NONE) return NULL;
    if (consume(tokens, (Token::Type)'(').type == Token::NONE)
        throw ParseError(format("expected '(' at %c", _pos), tokens[_pos]);
    auto exp = parse_expression(tokens);
    if (!exp)
        throw ParseError(format("exprected 'expression' at %d", _pos),
                         tokens[_pos]);
    if (consume(tokens, (Token::Type)')').type == Token::NONE)
        throw ParseError(format("expected ')' at %d", _pos), tokens[_pos]);
    Statement *body = parse_statement(tokens);
    return new WhileSt(exp, body);
}

Statement *Parser::parse_callst(vector<Token> &tokens) {
    Token id_token = tokens[_pos];
    Token lp = tokens[_pos + 1];
    if (id_token.type != Token::TK_ID || lp.type != (Token::Type)'(')
        return NULL;
    _pos++;
    vector<Expression *> args = parse_arg(tokens);
    if (consume(tokens, (Token::Type)';').type == Token::NONE)
        throw ParseError(format("expected ';'"), tokens[_pos]);
    return new CallFuncSt(id_token.id, args);
}

Expression *Parser::parse_eq(vector<Token> &tokens) {
    Expression *exp = parse_add(tokens);
    if (consume(tokens, Token::TK_EQ).type != Token::NONE) {
        return new EQExp(exp, parse_eq(tokens));
    } else if (consume(tokens, Token::TK_NE).type != Token::NONE) {
        return new NEExp(exp, parse_eq(tokens));
    } else if (consume(tokens, Token::TK_LE).type != Token::NONE) {
        return new LEExp(exp, parse_eq(tokens));
    } else if (consume(tokens, Token::TK_GE).type != Token::NONE) {
        return new GEExp(exp, parse_eq(tokens));
    } else if (consume(tokens, (Token::Type)'<').type != Token::NONE) {
        return new LTExp(exp, parse_eq(tokens));
    } else if (consume(tokens, (Token::Type)'>').type != Token::NONE) {
        return new GTExp(exp, parse_eq(tokens));
    } else {
        return exp;
    }
}

Expression *Parser::parse_add(vector<Token> &tokens) {
    Expression *exp = parse_mul(tokens);
    if (consume(tokens, (Token::Type)'+').type != Token::NONE) {
        return new AddExp(exp, parse_add(tokens));
    } else if (consume(tokens, (Token::Type)'-').type != Token::NONE) {
        return new SubExp(exp, parse_add(tokens));
    } else {
        return exp;
    }
}
Expression *Parser::parse_mul(vector<Token> &tokens) {
    Expression *exp = parse_unary(tokens);
    if (consume(tokens, (Token::Type)'*').type != Token::NONE) {
        return new MulExp(exp, parse_mul(tokens));
    } else if (consume(tokens, (Token::Type)'/').type != Token::NONE) {
        return new DivExp(exp, parse_mul(tokens));
    } else if (consume(tokens, (Token::Type)'%').type != Token::NONE) {
        return new ModExp(exp, parse_mul(tokens));
    } else {
        return exp;
    }
}

Expression *Parser::parse_unary(vector<Token> &tokens) {
    if (consume(tokens, (Token::Type)'-').type != Token::NONE) {
        return new SubExp(new IntExp(0), parse_array_index(tokens));
    } else if (consume(tokens, (Token::Type)'*').type != Token::NONE) {
        return new Access(parse_unary(tokens));
    } else if (consume(tokens, (Token::Type)'&').type != Token::NONE) {
        return new Address(parse_unary(tokens));
    } else {
        return parse_array_index(tokens);
    }
}

Expression *Parser::parse_array_index(vector<Token> &tokens) {
    Expression *pointer = parse_term(tokens);
    if (consume(tokens, (Token::Type)'[').type != Token::NONE) {
        Expression *index = parse_expression(tokens);
        if (consume(tokens, (Token::Type)']').type != Token::NONE)
            return new ArrayIndex(pointer, index);
        else
            throw ParseError(format("expected ']'"), tokens[_pos]);
    }
    return pointer;
}

Expression *Parser::parse_term(vector<Token> &tokens) {
    Token token;
    Expression *exp;
    if ((exp = parse_call(tokens))) return exp;
    if ((exp = parse_integer(tokens))) return exp;
    if ((token = consume(tokens, Token::TK_ID)).type != Token::NONE) {
        return new Variable(token.id);
    }
    if (consume(tokens, (Token::Type)'(').type != Token::NONE) {
        exp = parse_expression(tokens);
        if (consume(tokens, (Token::Type)')').type != Token::NONE) {
            return exp;
        } else {
            throw ParseError("expected ')'", tokens[_pos]);
        }
    }
    return NULL;
}

Expression *Parser::parse_expression(vector<Token> &tokens) {
    Expression *exp;
    if ((exp = parse_assign(tokens))) return exp;
    return NULL;
}

Expression *Parser::parse_assign(vector<Token> &tokens) {
    Expression *exp;
    if ((exp = parse_eq(tokens))) {
        if (consume(tokens, (Token::Type)'=').type != Token::NONE) {
            return new Assign(exp, parse_expression(tokens));
        } else {
            return exp;
        }
    }
    return NULL;
}

Expression *Parser::parse_integer(vector<Token> &tokens) {
    Token token = consume(tokens, Token::TK_FLOAT);
    if (token.type != Token::NONE) return new FloatExp(token.float_val);
    token = consume(tokens, Token::TK_INT);
    if (token.type == 0) return NULL;
    return new IntExp(token.int_val);
}

vector<Expression *> Parser::parse_arg(vector<Token> &tokens) {
    vector<Expression *> exprs;
    if (consume(tokens, (Token::Type)'(').type == Token::NONE)
        throw ParseError(format("expected '(' at %d", _pos), tokens[_pos]);
    auto exp = parse_expression(tokens);
    if (exp) {
        exprs.push_back(exp);
        while (consume(tokens, (Token::Type)',').type != Token::NONE) {
            exp = parse_expression(tokens);
            if (!exp)
                throw ParseError(format("expected 'expression' at %d", _pos),
                                 tokens[_pos]);
            exprs.push_back(exp);
        }
    }
    if (consume(tokens, (Token::Type)')').type == Token::NONE)
        throw ParseError(format("expected ')' at %d", _pos), tokens[_pos]);
    return exprs;
}

Expression *Parser::parse_call(vector<Token> &tokens) {
    Token id_token = tokens[_pos];
    Token lp = tokens[_pos + 1];
    if (id_token.type != Token::TK_ID || lp.type != (Token::Type)'(')
        return NULL;
    _pos++;
    vector<Expression *> args = parse_arg(tokens);
    return new CallFuncExp(id_token.id, args);
}

Token Parser::consume(vector<Token> &tokens, Token::Type type) {
    if (tokens[_pos].type == type) {
        _pos++;
        return tokens[_pos - 1];
    } else {
        return Token::none();
    }
}
