/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/parser.hpp"

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

vector<Function *> Parser::parse(vector<Token> &tokens) {
    _pos = 0;
    _struct_defs.clear();
    g_struct_defs.clear();
    while (consume(tokens, Token::TK_EOF).type == Token::NONE) {
        if (tokens[_pos].type == Token::KW_STRUCT) {
            parse_struct_def(tokens);
        } else {
            _functions.push_back(parse_function(tokens));
        }
    }
    return _functions;
}

void Parser::parse_struct_def(vector<Token> &tokens) {
    consume(tokens, Token::KW_STRUCT);
    Token name_tok = consume(tokens, Token::TK_ID);
    if (name_tok.type == Token::NONE)
        throw ParseError("expected struct name", tokens[_pos]);

    if (consume(tokens, (Token::Type)'{').type == Token::NONE)
        throw ParseError("expected '{' in struct definition", tokens[_pos]);

    StructDefInfo sdef;
    sdef.name = name_tok.id;

    while (tokens[_pos].type != (Token::Type)'}') {
        if (tokens[_pos].type == Token::KW_VAR) {
            // field: var name: type;
            _pos++;
            Token fname = consume(tokens, Token::TK_ID);
            if (fname.type == Token::NONE)
                throw ParseError("expected field name", tokens[_pos]);
            if (consume(tokens, (Token::Type)':').type == Token::NONE)
                throw ParseError("expected ':' in field declaration", tokens[_pos]);
            if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
                string field_struct = tokens[_pos].id;
                _pos++;
                if (consume(tokens, (Token::Type)';').type == Token::NONE)
                    throw ParseError("expected ';' after field declaration", tokens[_pos]);
                sdef.fields.push_back({fname.id, VarType::STRUCT, field_struct});
            } else if (is_type_keyword(tokens[_pos].type)) {
                VarType ftype = token_to_vartype(tokens[_pos].type);
                _pos++;
                if (consume(tokens, (Token::Type)';').type == Token::NONE)
                    throw ParseError("expected ';' after field declaration", tokens[_pos]);
                sdef.fields.push_back({fname.id, ftype, ""});
            } else {
                throw ParseError("expected type keyword or struct name in field declaration",
                                 tokens[_pos]);
            }
        } else if (tokens[_pos].type == Token::KW_CONSTRUCTOR) {
            // constructor(params) { body }
            _pos++;
            auto params_decl = parse_declargs(tokens);
            for (auto *d : params_decl)
                if (d->getType() == VarType::STRUCT)
                    throw ParseError("struct types cannot be used as constructor parameters",
                                     tokens[_pos]);
            vector<pair<string, VarType>> params;
            for (auto *d : params_decl)
                params.push_back({d->getId(), d->getType()});
            auto body = parse_block(tokens);
            auto ci = new ConstructorInfo{params, body};
            sdef.constructor = ci;
        } else {
            throw ParseError("expected 'var' or 'constructor' in struct body", tokens[_pos]);
        }
    }

    if (consume(tokens, (Token::Type)'}').type == Token::NONE)
        throw ParseError("expected '}' to close struct", tokens[_pos]);

    _struct_defs[name_tok.id] = sdef;
    g_struct_defs[name_tok.id] = sdef;
}

Function *Parser::parse_function(vector<Token> &tokens) {
    if (consume(tokens, Token::KW_FUNC).type == Token::NONE) {
        if (consume(tokens, Token::KW_IMPORT).type == Token::NONE) {
            throw ParseError(format("position: %d", _pos), tokens[_pos]);
        }
        // C header import: import "stdio.h";
        if (tokens[_pos].type == Token::TK_RAWSTRING) {
            string header_path = tokens[_pos].id;
            _pos++;
            if (consume(tokens, (Token::Type)';').type == Token::NONE)
                throw ParseError("expected ';' after header path", tokens[_pos]);
            return new ImportCHeader(header_path);
        }
        // Dorothy-style import: import funcname;
        {
            Token id_token = consume(tokens, Token::TK_ID);
            vector<DeclVar *> args;
            if (id_token.type == Token::NONE)
                throw ParseError("expected ID or header string", tokens[_pos]);
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
    VarType ret_type = VarType::LONG;
    string ret_struct_name = "";
    bool has_explicit_ret_type = false;
    if (consume(tokens, (Token::Type)':').type != Token::NONE) {
        // func name(): type {}
        has_explicit_ret_type = true;
        if (is_type_keyword(tokens[_pos].type)) {
            ret_type = token_to_vartype(tokens[_pos].type);
            _pos++;
        } else if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
            ret_type = VarType::STRUCT;
            ret_struct_name = tokens[_pos].id;
            _pos++;
        } else {
            throw ParseError("expected type keyword after ':' in function return type",
                             tokens[_pos]);
        }
    } else if (consume(tokens, Token::TK_ARROW).type != Token::NONE) {
        // func name() -> StructName {} (legacy struct return syntax)
        has_explicit_ret_type = true;
        Token struct_tok = consume(tokens, Token::TK_ID);
        if (struct_tok.type == Token::NONE)
            throw ParseError("expected struct name after '->'", tokens[_pos]);
        if (!_struct_defs.count(struct_tok.id))
            throw ParseError("unknown struct type: " + struct_tok.id, tokens[_pos]);
        ret_type = VarType::STRUCT;
        ret_struct_name = struct_tok.id;
    }
    auto block = parse_block(tokens);
    return new Function(id_token.id, declargs, block, ret_type, ret_struct_name,
                        has_explicit_ret_type);
}

vector<DeclVar *> Parser::parse_declargs(vector<Token> &tokens) {
    vector<DeclVar *> declargs;
    if (consume(tokens, (Token::Type)'(').type == Token::NONE)
        throw ParseError(format("position: %d", _pos), tokens[_pos]);
    // Parameters use "name: type" syntax
    if (tokens[_pos].type == Token::TK_ID) {
        declargs.push_back(parse_declparam(tokens));
        while (consume(tokens, (Token::Type)',').type == (Token::Type)',') {
            declargs.push_back(parse_declparam(tokens));
        }
    }
    if (consume(tokens, (Token::Type)')').type == Token::NONE)
        throw ParseError(format("position: %d", _pos), tokens[_pos]);
    return declargs;
}

DeclVar *Parser::parse_declparam(vector<Token> &tokens) {
    Token id_token = consume(tokens, Token::TK_ID);
    if (id_token.type == Token::NONE)
        throw ParseError(format("expected parameter name at %d", _pos), tokens[_pos]);
    if (consume(tokens, (Token::Type)':').type == Token::NONE)
        throw ParseError("expected ':' after parameter name", tokens[_pos]);
    if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
        string struct_name = tokens[_pos].id;
        _pos++;
        return new DeclVar(id_token.id, VarType::STRUCT, false, struct_name);
    }
    if (!is_type_keyword(tokens[_pos].type))
        throw ParseError("expected type keyword after ':'", tokens[_pos]);
    VarType vtype = token_to_vartype(tokens[_pos].type);
    _pos++;
    return new DeclVar(id_token.id, vtype);
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
    // String literal shorthand: "hello" expands to {'h','e','l','l','o',0}
    if (tokens[_pos].type == Token::TK_RAWSTRING) {
        string s = tokens[_pos].id;
        _pos++;
        vector<Expression *> exprs;
        for (unsigned char c : s) exprs.push_back(new IntExp(c));
        exprs.push_back(new IntExp(0));
        return exprs;
    }
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
    Token::Type kw = tokens[_pos].type;
    if (kw != Token::KW_VAR && kw != Token::KW_LET) return NULL;

    bool is_const = (kw == Token::KW_LET);
    _pos++;

    Token id_token = consume(tokens, Token::TK_ID);
    if (id_token.type == Token::NONE)
        throw ParseError(format("expected identifier at %d", _pos), tokens[_pos]);

    // ===== Type inference: var/let x = expr; (no ':' type annotation) =====
    if (tokens[_pos].type == (Token::Type)'=') {
        _pos++;  // consume '='

        // String literal → char[N] array (size = len + 1 for null terminator)
        if (tokens[_pos].type == Token::TK_RAWSTRING) {
            string s = tokens[_pos].id;
            _pos++;
            if (consume(tokens, (Token::Type)';').type == Token::NONE)
                throw ParseError("expected ';'", tokens[_pos]);
            int sz = (int)s.size() + 1;
            vector<Expression *> vals;
            for (unsigned char c : s) vals.push_back(new IntExp(c));
            vals.push_back(new IntExp(0));
            return new DeclVarSt(
                new InitializedDeclArrayVar(id_token.id, sz, vals, VarType::CHAR, is_const));
        }

        Expression *init = parse_expression(tokens);
        if (!init)
            throw ParseError("expected expression after '='", tokens[_pos]);
        if (consume(tokens, (Token::Type)';').type == Token::NONE)
            throw ParseError("expected ';'", tokens[_pos]);

        auto* si = dynamic_cast<StructInit*>(init);
        if (si) {
            return new DeclVarSt(new InitializedDeclVar(
                id_token.id, VarType::STRUCT, is_const, init, si->getStructName()));
        }

        // All other expressions: defer type resolution to llvm_emit via INFERRED
        return new DeclVarSt(
            new InitializedDeclVar(id_token.id, VarType::INFERRED, is_const, init));
    }
    // ===== End type inference =====

    if (consume(tokens, (Token::Type)':').type == Token::NONE)
        throw ParseError("expected ':' or '=' after variable name", tokens[_pos]);

    // Struct type: var name: StructName = StructName(field=val, ...);
    if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
        string struct_name = tokens[_pos].id;
        _pos++;
        if (consume(tokens, (Token::Type)'=').type == Token::NONE)
            throw ParseError("struct variable requires an initializer", tokens[_pos]);
        auto init = parse_expression(tokens);
        if (!init) throw ParseError("expected struct initializer expression", tokens[_pos]);
        if (consume(tokens, (Token::Type)';').type == Token::NONE)
            throw ParseError("expected ';'", tokens[_pos]);
        return new DeclVarSt(
            new InitializedDeclVar(id_token.id, VarType::STRUCT, is_const, init, struct_name));
    }

    if (!is_type_keyword(tokens[_pos].type))
        throw ParseError("expected type keyword or struct name after ':'", tokens[_pos]);
    VarType vtype = token_to_vartype(tokens[_pos].type);
    _pos++;

    // Array: var name: type[N] [= {vals}];
    if (consume(tokens, (Token::Type)'[').type != Token::NONE) {
        Token num_token = consume(tokens, Token::TK_INT);
        if (num_token.type == Token::NONE)
            throw ParseError("expected array size", tokens[_pos]);
        if (consume(tokens, (Token::Type)']').type == Token::NONE)
            throw ParseError("expected ']'", tokens[_pos]);
        if (consume(tokens, (Token::Type)'=').type != Token::NONE) {
            auto vals = parse_array_initializer(tokens);
            if (consume(tokens, (Token::Type)';').type == Token::NONE)
                throw ParseError("expected ';'", tokens[_pos]);
            return new DeclVarSt(
                new InitializedDeclArrayVar(id_token.id, num_token.int_val, vals, vtype));
        }
        if (is_const)
            throw ParseError("'let' array requires an initializer", tokens[_pos]);
        if (consume(tokens, (Token::Type)';').type == Token::NONE)
            throw ParseError("expected ';'", tokens[_pos]);
        return new DeclVarSt(new DeclArrayVar(id_token.id, num_token.int_val, vtype));
    }

    // Scalar: var name: type [= expr];
    if (consume(tokens, (Token::Type)'=').type != Token::NONE) {
        auto init = parse_expression(tokens);
        if (!init)
            throw ParseError("expected expression after '='", tokens[_pos]);
        if (consume(tokens, (Token::Type)';').type == Token::NONE)
            throw ParseError("expected ';'", tokens[_pos]);
        return new DeclVarSt(new InitializedDeclVar(id_token.id, vtype, is_const, init));
    }
    if (is_const)
        throw ParseError("'let' declaration requires an initializer", tokens[_pos]);
    if (consume(tokens, (Token::Type)';').type == Token::NONE)
        throw ParseError("expected ';'", tokens[_pos]);
    return new DeclVarSt(new DeclVar(id_token.id, vtype, false));
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
    Expression *base = parse_term(tokens);
    while (true) {
        if (consume(tokens, (Token::Type)'[').type != Token::NONE) {
            Expression *index = parse_expression(tokens);
            if (consume(tokens, (Token::Type)']').type != Token::NONE) {
                base = new ArrayIndex(base, index);
            } else {
                throw ParseError(format("expected ']'"), tokens[_pos]);
            }
        } else if (consume(tokens, (Token::Type)'.').type != Token::NONE) {
            Token member = consume(tokens, Token::TK_ID);
            if (member.type == Token::NONE)
                throw ParseError("expected member name after '.'", tokens[_pos]);
            base = new MemberAccess(base, member.id);
        } else {
            break;
        }
    }
    return base;
}

Expression *Parser::parse_struct_init(vector<Token> &tokens) {
    if (tokens[_pos].type != Token::TK_ID) return NULL;
    if (!_struct_defs.count(tokens[_pos].id)) return NULL;
    if (tokens[_pos + 1].type != (Token::Type)'(') return NULL;
    string struct_name = tokens[_pos].id;
    _pos += 2;  // consume StructName and (
    vector<Expression *> args;
    while (tokens[_pos].type != (Token::Type)')') {
        auto val = parse_expression(tokens);
        if (!val) throw ParseError("expected expression in struct initializer", tokens[_pos]);
        args.push_back(val);
        if (tokens[_pos].type != (Token::Type)')') {
            if (consume(tokens, (Token::Type)',').type == Token::NONE)
                throw ParseError("expected ',' or ')' in struct initializer", tokens[_pos]);
        }
    }
    consume(tokens, (Token::Type)')');
    return new StructInit(struct_name, std::move(args));
}

Expression *Parser::parse_term(vector<Token> &tokens) {
    Token token;
    Expression *exp;
    if ((exp = parse_struct_init(tokens))) return exp;
    if ((exp = parse_call(tokens))) return exp;
    if ((exp = parse_integer(tokens))) return exp;
    if (consume(tokens, Token::KW_THIS).type != Token::NONE) return new ThisExpr();
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
