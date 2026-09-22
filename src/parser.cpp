#include <fstream>
#include <filesystem>
#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include "../include/utils.hpp"

namespace fs = std::filesystem;

static bool is_type_keyword(Token::Type t) {
    return t == Token::KW_INT || t == Token::KW_CHAR || t == Token::KW_LONG ||
           t == Token::KW_FLOAT || t == Token::KW_DOUBLE || t == Token::KW_STRING;
}

static VarType token_to_vartype(Token::Type t) {
    switch (t) {
        case Token::KW_CHAR:   return VarType::CHAR;
        case Token::KW_INT:    return VarType::INT;
        case Token::KW_LONG:   return VarType::LONG;
        case Token::KW_FLOAT:  return VarType::FLOAT;
        case Token::KW_DOUBLE: return VarType::DOUBLE;
        case Token::KW_STRING: return VarType::STRING;
        default:               return VarType::LONG;
    }
}

static bool is_dorothy_import(const string &path) {
    return (path.size() >= 8 && path.substr(path.size() - 8) == ".dorothy");
}

string Parser::resolve_path(const string &path, const string &base_dir) {
    fs::path p(path);
    if (p.is_absolute()) {
        if (fs::exists(p)) {
            return fs::weakly_canonical(p).string();
        }
        return path;
    }
    if (!base_dir.empty()) {
        fs::path candidate = fs::path(base_dir) / p;
        if (fs::exists(candidate)) {
            return fs::weakly_canonical(candidate).string();
        }
    }
    if (fs::exists(p)) {
        return fs::weakly_canonical(p).string();
    }
    if (!base_dir.empty()) {
        return (fs::path(base_dir) / p).string();
    }
    return p.string();
}

void Parser::import_dorothy_file(const string &file_path, const string &base_dir, Token import_tok) {
    string resolved = resolve_path(file_path, base_dir);
    if (!fs::exists(resolved)) {
        throw ParseError("cannot open file: " + file_path, import_tok);
    }
    string canonical_path = fs::weakly_canonical(fs::path(resolved)).string();
    if (_loaded_files.count(canonical_path)) {
        // Already loaded, skip to avoid circular/duplicate imports
        return;
    }
    _loaded_files.insert(canonical_path);

    std::ifstream ifs(canonical_path);
    if (!ifs.is_open()) {
        throw ParseError("cannot open file: " + file_path, import_tok);
    }
    string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    Lexer lexer;
    auto tokens = lexer.lex(content.c_str());

    string next_base_dir = fs::path(canonical_path).parent_path().string();
    parse_top_level(tokens, next_base_dir);
}

void Parser::parse_top_level(vector<Token> &tokens, const string &base_dir) {
    int saved_pos = _pos;
    string saved_base_dir = _base_dir;
    _pos = 0;
    _base_dir = base_dir;

    // Pre-scan for forward references of struct and class names
    for (int i = 0; i < (int)tokens.size(); i++) {
        if (tokens[i].type == Token::KW_CLASS) {
            if (i + 1 < (int)tokens.size() && tokens[i + 1].type == Token::TK_ID) {
                string cname = tokens[i + 1].id;
                if (!_class_defs.count(cname)) {
                    ClassDefInfo cdef;
                    cdef.name = cname;
                    _class_defs[cname] = cdef;
                    g_class_defs[cname] = cdef;
                }
            }
        } else if (tokens[i].type == Token::KW_ABSTRACT) {
            if (i + 2 < (int)tokens.size() && tokens[i + 1].type == Token::KW_CLASS && tokens[i + 2].type == Token::TK_ID) {
                string cname = tokens[i + 2].id;
                if (!_class_defs.count(cname)) {
                    ClassDefInfo cdef;
                    cdef.name = cname;
                    cdef.is_abstract = true;
                    _class_defs[cname] = cdef;
                    g_class_defs[cname] = cdef;
                }
            }
        } else if (tokens[i].type == Token::KW_STRUCT) {
            if (i + 1 < (int)tokens.size() && tokens[i + 1].type == Token::TK_ID) {
                string sname = tokens[i + 1].id;
                if (!_struct_defs.count(sname)) {
                    StructDefInfo sdef;
                    sdef.name = sname;
                    _struct_defs[sname] = sdef;
                    g_struct_defs[sname] = sdef;
                }
            }
        }
    }

    while (consume(tokens, Token::TK_EOF).type == Token::NONE) {
        if (tokens[_pos].type == Token::KW_STRUCT) {
            parse_struct_def(tokens);
        } else if (tokens[_pos].type == Token::KW_CLASS ||
                   (tokens[_pos].type == Token::KW_ABSTRACT &&
                    _pos + 1 < (int)tokens.size() &&
                    tokens[_pos + 1].type == Token::KW_CLASS)) {
            parse_class_def(tokens);
        } else if (tokens[_pos].type == Token::KW_IMPORT &&
                   _pos + 1 < (int)tokens.size() &&
                   tokens[_pos + 1].type == Token::TK_RAWSTRING &&
                   is_dorothy_import(tokens[_pos + 1].id)) {
            Token import_tok = consume(tokens, Token::KW_IMPORT);
            Token path_tok = consume(tokens, Token::TK_RAWSTRING);
            if (consume(tokens, (Token::Type)';').type == Token::NONE) {
                throw ParseError("expected ';' after import path", tokens[_pos]);
            }
            import_dorothy_file(path_tok.id, _base_dir, import_tok);
        } else {
            Function *func = parse_function(tokens);
            if (!func->isImport()) {
                const string &name = func->getName();
                if (_defined_functions.count(name)) {
                    if (name == "main") {
                        throw ParseError("multiple definition of 'main' function", tokens[_pos]);
                    } else {
                        throw ParseError("multiple definition of function '" + name + "'", tokens[_pos]);
                    }
                }
                _defined_functions.insert(name);
            }
            _functions.push_back(func);
        }
    }

    _pos = saved_pos;
    _base_dir = saved_base_dir;
}

vector<Function *> Parser::parse(vector<Token> &tokens, const string &base_dir) {
    _pos = 0;
    _struct_defs.clear();
    g_struct_defs.clear();
    _class_defs.clear();
    g_class_defs.clear();
    _functions.clear();
    _defined_functions.clear();
    _defined_types.clear();
    _loaded_files.clear();
    _base_dir = base_dir;

    parse_top_level(tokens, base_dir);
    return _functions;
}

vector<Function *> Parser::parse_file(const string &filepath) {
    fs::path p(filepath);
    if (!fs::exists(p)) {
        throw runtime_error("file not found: " + filepath);
    }
    string canonical_path = fs::weakly_canonical(p).string();
    _pos = 0;
    _struct_defs.clear();
    g_struct_defs.clear();
    _class_defs.clear();
    g_class_defs.clear();
    _functions.clear();
    _defined_functions.clear();
    _defined_types.clear();
    _loaded_files.clear();
    _loaded_files.insert(canonical_path);

    std::ifstream ifs(canonical_path);
    if (!ifs.is_open()) {
        throw runtime_error("cannot open file: " + filepath);
    }
    string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    Lexer lexer;
    auto tokens = lexer.lex(content.c_str());

    string base_dir = p.parent_path().string();
    parse_top_level(tokens, base_dir);
    return _functions;
}

void Parser::parse_struct_def(vector<Token> &tokens) {
    consume(tokens, Token::KW_STRUCT);
    Token name_tok = consume(tokens, Token::TK_ID);
    if (name_tok.type == Token::NONE)
        throw ParseError("expected struct name", tokens[_pos]);

    string struct_name = name_tok.id;
    if (_defined_types.count(struct_name))
        throw ParseError("redefinition of type '" + struct_name + "'", name_tok);

    if (consume(tokens, (Token::Type)'{').type == Token::NONE)
        throw ParseError("expected '{' in struct definition", tokens[_pos]);

    StructDefInfo sdef;
    sdef.name = struct_name;

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

    _struct_defs[struct_name] = sdef;
    g_struct_defs[struct_name] = sdef;
    _defined_types.insert(struct_name);
}

void Parser::parse_class_def(vector<Token> &tokens) {
    bool is_abstract = false;
    if (tokens[_pos].type == Token::KW_ABSTRACT) {
        is_abstract = true;
        _pos++;
    }
    Token class_tok = consume(tokens, Token::KW_CLASS);
    if (class_tok.type == Token::NONE)
        throw ParseError("expected 'class'", tokens[_pos]);

    Token name_tok = consume(tokens, Token::TK_ID);
    if (name_tok.type == Token::NONE)
        throw ParseError("expected class name", tokens[_pos]);

    string class_name = name_tok.id;
    if (_defined_types.count(class_name))
        throw ParseError("redefinition of type '" + class_name + "'", name_tok);

    ClassDefInfo cdef;
    cdef.name = class_name;
    cdef.is_abstract = is_abstract;

    // Inheritance: class A: B
    if (consume(tokens, (Token::Type)':').type != Token::NONE) {
        Token base_tok = consume(tokens, Token::TK_ID);
        if (base_tok.type == Token::NONE)
            throw ParseError("expected base class name after ':'", tokens[_pos]);
        string base_name = base_tok.id;
        if (!_class_defs.count(base_name))
            throw ParseError("unknown base class: " + base_name, base_tok);
        cdef.base_class = base_name;
        const auto &base_def = _class_defs[base_name];

        // Inherit fields
        cdef.fields = base_def.fields;

        // Inherit vtable methods
        cdef.vtable_methods = base_def.vtable_methods;
        for (auto *m : cdef.vtable_methods) {
            cdef.methods[m->name] = m;
        }
    }

    if (consume(tokens, (Token::Type)'{').type == Token::NONE)
        throw ParseError("expected '{' in class definition", tokens[_pos]);

    _class_defs[class_name] = cdef;
    g_class_defs[class_name] = cdef;

    while (tokens[_pos].type != (Token::Type)'}' && tokens[_pos].type != Token::TK_EOF) {
        if (tokens[_pos].type == Token::KW_VAR) {
            // var fname: type;
            _pos++;
            Token fname = consume(tokens, Token::TK_ID);
            if (fname.type == Token::NONE)
                throw ParseError("expected field name", tokens[_pos]);
            if (consume(tokens, (Token::Type)':').type == Token::NONE)
                throw ParseError("expected ':' in field declaration", tokens[_pos]);

            if (tokens[_pos].type == Token::TK_ID && _class_defs.count(tokens[_pos].id)) {
                string field_class = tokens[_pos].id;
                _pos++;
                bool is_nullable = false;
                if (tokens[_pos].type == (Token::Type)'?') {
                    is_nullable = true;
                    _pos++;
                }
                if (consume(tokens, (Token::Type)';').type == Token::NONE)
                    throw ParseError("expected ';' after field declaration", tokens[_pos]);
                if (cdef.fieldIndex(fname.id) >= 0)
                    throw ParseError("duplicate field '" + fname.id + "' in class " + class_name, fname);
                cdef.fields.push_back({fname.id, VarType::CLASS, field_class, is_nullable});
            } else if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
                string field_struct = tokens[_pos].id;
                _pos++;
                if (consume(tokens, (Token::Type)';').type == Token::NONE)
                    throw ParseError("expected ';' after field declaration", tokens[_pos]);
                if (cdef.fieldIndex(fname.id) >= 0)
                    throw ParseError("duplicate field '" + fname.id + "' in class " + class_name, fname);
                cdef.fields.push_back({fname.id, VarType::STRUCT, field_struct, false});
            } else if (is_type_keyword(tokens[_pos].type)) {
                VarType ftype = token_to_vartype(tokens[_pos].type);
                _pos++;
                if (consume(tokens, (Token::Type)';').type == Token::NONE)
                    throw ParseError("expected ';' after field declaration", tokens[_pos]);
                if (cdef.fieldIndex(fname.id) >= 0)
                    throw ParseError("duplicate field '" + fname.id + "' in class " + class_name, fname);
                cdef.fields.push_back({fname.id, ftype, "", false});
            } else {
                throw ParseError("expected type in field declaration", tokens[_pos]);
            }
        } else if (tokens[_pos].type == Token::KW_CONSTRUCTOR) {
            // constructor(params) { body }
            _pos++;
            auto params_decl = parse_declargs(tokens);
            vector<pair<string, VarType>> params;
            for (auto *d : params_decl)
                params.push_back({d->getId(), d->getType()});
            auto body = parse_block(tokens);
            cdef.constructor = new ConstructorInfo{params, body};
        } else if (tokens[_pos].type == Token::KW_ABSTRACT) {
            // abstract func mname(params): ret; OR abstract func mname(params) -> ret;
            if (!is_abstract) {
                throw ParseError("cannot declare abstract method in non-abstract class '" + class_name + "'", tokens[_pos]);
            }
            _pos++;
            if (consume(tokens, Token::KW_FUNC).type == Token::NONE)
                throw ParseError("expected 'func' after 'abstract'", tokens[_pos]);
            Token mname = consume(tokens, Token::TK_ID);
            if (mname.type == Token::NONE)
                throw ParseError("expected method name", tokens[_pos]);
            auto params_decl = parse_declargs(tokens);
            VarType ret_type = VarType::LONG;
            string ret_type_name = "";
            bool is_ret_nullable = false;
            if (consume(tokens, (Token::Type)':').type != Token::NONE ||
                consume(tokens, Token::TK_ARROW).type != Token::NONE) {
                if (is_type_keyword(tokens[_pos].type)) {
                    ret_type = token_to_vartype(tokens[_pos].type);
                    _pos++;
                } else if (tokens[_pos].type == Token::TK_ID && _class_defs.count(tokens[_pos].id)) {
                    ret_type = VarType::CLASS;
                    ret_type_name = tokens[_pos].id;
                    _pos++;
                    if (tokens[_pos].type == (Token::Type)'?') {
                        is_ret_nullable = true;
                        _pos++;
                    }
                } else if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
                    ret_type = VarType::STRUCT;
                    ret_type_name = tokens[_pos].id;
                    _pos++;
                } else {
                    throw ParseError("expected return type after ':' or '->'", tokens[_pos]);
                }
            }
            if (consume(tokens, (Token::Type)';').type == Token::NONE)
                throw ParseError("expected ';' after abstract method declaration", tokens[_pos]);

            auto *minfo = new MethodInfo();
            minfo->name = mname.id;
            minfo->params = params_decl;
            minfo->ret_type = ret_type;
            minfo->ret_type_name = ret_type_name;
            minfo->is_ret_nullable = is_ret_nullable;
            minfo->body = nullptr;
            minfo->is_abstract = true;
            minfo->is_override = false;
            minfo->class_name = class_name;

            int existing_vtable_idx = cdef.getMethodVtableIndex(mname.id);
            if (existing_vtable_idx >= 0) {
                minfo->vtable_index = existing_vtable_idx;
                cdef.vtable_methods[existing_vtable_idx] = minfo;
            } else {
                minfo->vtable_index = (int)cdef.vtable_methods.size();
                cdef.vtable_methods.push_back(minfo);
            }
            cdef.methods[mname.id] = minfo;
        } else if (tokens[_pos].type == Token::KW_OVERRIDE || tokens[_pos].type == Token::KW_FUNC) {
            bool is_override = (tokens[_pos].type == Token::KW_OVERRIDE);
            if (is_override) {
                _pos++;
            }
            if (consume(tokens, Token::KW_FUNC).type == Token::NONE)
                throw ParseError("expected 'func' in method declaration", tokens[_pos]);
            Token mname = consume(tokens, Token::TK_ID);
            if (mname.type == Token::NONE)
                throw ParseError("expected method name", tokens[_pos]);
            auto params_decl = parse_declargs(tokens);

            // Check if this is a constructor: func ClassName(...) { ... }
            if (mname.id == class_name) {
                if (is_override) {
                    throw ParseError("constructor cannot be marked 'override'", mname);
                }
                auto body = parse_block(tokens);
                vector<pair<string, VarType>> params;
                for (auto *d : params_decl)
                    params.push_back({d->getId(), d->getType()});
                cdef.constructor = new ConstructorInfo{params, body};
            } else {
                VarType ret_type = VarType::LONG;
                string ret_type_name = "";
                bool is_ret_nullable = false;
                if (consume(tokens, (Token::Type)':').type != Token::NONE ||
                    consume(tokens, Token::TK_ARROW).type != Token::NONE) {
                    if (is_type_keyword(tokens[_pos].type)) {
                        ret_type = token_to_vartype(tokens[_pos].type);
                        _pos++;
                    } else if (tokens[_pos].type == Token::TK_ID && _class_defs.count(tokens[_pos].id)) {
                        ret_type = VarType::CLASS;
                        ret_type_name = tokens[_pos].id;
                        _pos++;
                        if (tokens[_pos].type == (Token::Type)'?') {
                            is_ret_nullable = true;
                            _pos++;
                        }
                    } else if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
                        ret_type = VarType::STRUCT;
                        ret_type_name = tokens[_pos].id;
                        _pos++;
                    } else {
                        throw ParseError("expected return type after ':' or '->'", tokens[_pos]);
                    }
                }
                auto body = parse_block(tokens);

                int existing_vtable_idx = cdef.getMethodVtableIndex(mname.id);
                if (is_override && existing_vtable_idx < 0) {
                    throw ParseError("method '" + mname.id + "' marked override does not override any base class method", mname);
                }

                auto *minfo = new MethodInfo();
                minfo->name = mname.id;
                minfo->params = params_decl;
                minfo->ret_type = ret_type;
                minfo->ret_type_name = ret_type_name;
                minfo->is_ret_nullable = is_ret_nullable;
                minfo->body = body;
                minfo->is_abstract = false;
                minfo->is_override = is_override || (existing_vtable_idx >= 0);
                minfo->class_name = class_name;

                if (existing_vtable_idx >= 0) {
                    minfo->vtable_index = existing_vtable_idx;
                    cdef.vtable_methods[existing_vtable_idx] = minfo;
                } else {
                    minfo->vtable_index = (int)cdef.vtable_methods.size();
                    cdef.vtable_methods.push_back(minfo);
                }
                cdef.methods[mname.id] = minfo;
            }
        } else {
            throw ParseError("unexpected token in class body", tokens[_pos]);
        }
    }

    if (consume(tokens, (Token::Type)'}').type == Token::NONE)
        throw ParseError("expected '}' to close class", tokens[_pos]);

    // Validation: Concrete class must implement all abstract methods
    if (!cdef.is_abstract) {
        for (auto *m : cdef.vtable_methods) {
            if (m->is_abstract) {
                throw ParseError("class '" + class_name + "' must implement abstract method '" + m->name + "'", name_tok);
            }
        }
    }

    _class_defs[class_name] = cdef;
    g_class_defs[class_name] = cdef;
    _defined_types.insert(class_name);
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
    bool is_ret_nullable = false;
    if (consume(tokens, (Token::Type)':').type != Token::NONE ||
        consume(tokens, Token::TK_ARROW).type != Token::NONE) {
        has_explicit_ret_type = true;
        if (is_type_keyword(tokens[_pos].type)) {
            ret_type = token_to_vartype(tokens[_pos].type);
            _pos++;
        } else if (tokens[_pos].type == Token::TK_ID && _class_defs.count(tokens[_pos].id)) {
            ret_type = VarType::CLASS;
            ret_struct_name = tokens[_pos].id;
            _pos++;
            if (tokens[_pos].type == (Token::Type)'?') {
                is_ret_nullable = true;
                _pos++;
            }
        } else if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
            ret_type = VarType::STRUCT;
            ret_struct_name = tokens[_pos].id;
            _pos++;
        } else {
            throw ParseError("expected type after ':' or '->'", tokens[_pos]);
        }
    }
    auto block = parse_block(tokens);
    return new Function(id_token.id, declargs, block, ret_type, ret_struct_name,
                        has_explicit_ret_type, is_ret_nullable);
}

vector<DeclVar *> Parser::parse_declargs(vector<Token> &tokens) {
    vector<DeclVar *> declargs;
    if (consume(tokens, (Token::Type)'(').type == Token::NONE)
        throw ParseError(format("position: %d", _pos), tokens[_pos]);
    // Parameters use "name: type" syntax, optionally prefixed with let or var
    if (tokens[_pos].type == Token::TK_ID ||
        tokens[_pos].type == Token::KW_LET ||
        tokens[_pos].type == Token::KW_VAR) {
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
    bool is_const = false;
    if (tokens[_pos].type == Token::KW_LET) {
        is_const = true;
        _pos++;
    } else if (tokens[_pos].type == Token::KW_VAR) {
        _pos++;
    }
    Token id_token = consume(tokens, Token::TK_ID);
    if (id_token.type == Token::NONE)
        throw ParseError(format("expected parameter name at %d", _pos), tokens[_pos]);
    if (consume(tokens, (Token::Type)':').type == Token::NONE)
        throw ParseError("expected ':' after parameter name", tokens[_pos]);
    if (tokens[_pos].type == Token::TK_ID && _class_defs.count(tokens[_pos].id)) {
        string class_name = tokens[_pos].id;
        _pos++;
        bool is_nullable = false;
        if (tokens[_pos].type == (Token::Type)'?') {
            is_nullable = true;
            _pos++;
        }
        return new DeclVar(id_token.id, VarType::CLASS, is_const, class_name, is_nullable);
    }
    if (tokens[_pos].type == Token::TK_ID && _struct_defs.count(tokens[_pos].id)) {
        string struct_name = tokens[_pos].id;
        _pos++;
        return new DeclVar(id_token.id, VarType::STRUCT, is_const, struct_name);
    }
    if (!is_type_keyword(tokens[_pos].type))
        throw ParseError("expected type keyword after ':'", tokens[_pos]);
    VarType vtype = token_to_vartype(tokens[_pos].type);
    _pos++;
    return new DeclVar(id_token.id, vtype, is_const);
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

        // String literal → String type (VarType::STRING)
        if (tokens[_pos].type == Token::TK_RAWSTRING) {
            string s = tokens[_pos].id;
            _pos++;
            if (consume(tokens, (Token::Type)';').type == Token::NONE)
                throw ParseError("expected ';'", tokens[_pos]);
            return new DeclVarSt(
                new InitializedDeclVar(id_token.id, VarType::STRING, is_const, new StringExp(s)));
        }

        Expression *init = parse_expression(tokens);
        if (!init)
            throw ParseError("expected expression after '='", tokens[_pos]);
        if (consume(tokens, (Token::Type)';').type == Token::NONE)
            throw ParseError("expected ';'", tokens[_pos]);

        auto* ci = dynamic_cast<ClassInit*>(init);
        if (ci) {
            return new DeclVarSt(new InitializedDeclVar(
                id_token.id, VarType::CLASS, is_const, init, ci->getClassName()));
        }

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

    // Class type: var name: ClassName [?] [= expr];
    if (tokens[_pos].type == Token::TK_ID && _class_defs.count(tokens[_pos].id)) {
        string class_name = tokens[_pos].id;
        _pos++;
        bool is_nullable = false;
        if (tokens[_pos].type == (Token::Type)'?') {
            is_nullable = true;
            _pos++;
        }
        if (consume(tokens, (Token::Type)'=').type != Token::NONE) {
            auto init = parse_expression(tokens);
            if (!init) throw ParseError("expected class initializer expression", tokens[_pos]);
            if (consume(tokens, (Token::Type)';').type == Token::NONE)
                throw ParseError("expected ';'", tokens[_pos]);
            if (!is_nullable && dynamic_cast<NullExp*>(init)) {
                throw ParseError("cannot assign null to non-nullable variable '" + id_token.id + "' of type '" + class_name + "'", id_token);
            }
            return new DeclVarSt(
                new InitializedDeclVar(id_token.id, VarType::CLASS, is_const, init, class_name, is_nullable));
        }
        if (is_const)
            throw ParseError("'let' class variable requires an initializer", tokens[_pos]);
        if (consume(tokens, (Token::Type)';').type == Token::NONE)
            throw ParseError("expected ';'", tokens[_pos]);
        if (!is_nullable) {
            throw ParseError("non-nullable variable '" + id_token.id + "' of type '" + class_name + "' must be initialized", id_token);
        }
        return new DeclVarSt(new InitializedDeclVar(id_token.id, VarType::CLASS, false, new NullExp(), class_name, true));
    }

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
        throw ParseError("expected type keyword or type name after ':'", tokens[_pos]);
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
            auto *cme = dynamic_cast<CallMethodExp*>(exp);
            if (cme) return new CallMethodSt(cme);
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
            if (tokens[_pos].type == (Token::Type)'(') {
                vector<Expression *> args = parse_arg(tokens);
                base = new CallMethodExp(base, member.id, std::move(args));
            } else {
                base = new MemberAccess(base, member.id);
            }
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

Expression *Parser::parse_class_init(vector<Token> &tokens) {
    if (tokens[_pos].type != Token::TK_ID) return NULL;
    if (!_class_defs.count(tokens[_pos].id)) return NULL;
    if (tokens[_pos + 1].type != (Token::Type)'(') return NULL;
    string class_name = tokens[_pos].id;
    if (_class_defs[class_name].is_abstract) {
        throw ParseError("cannot instantiate abstract class '" + class_name + "'", tokens[_pos]);
    }
    _pos += 2;  // consume ClassName and (
    vector<Expression *> args;
    while (tokens[_pos].type != (Token::Type)')') {
        auto val = parse_expression(tokens);
        if (!val) throw ParseError("expected expression in class constructor call", tokens[_pos]);
        args.push_back(val);
        if (tokens[_pos].type != (Token::Type)')') {
            if (consume(tokens, (Token::Type)',').type == Token::NONE)
                throw ParseError("expected ',' or ')' in class constructor call", tokens[_pos]);
        }
    }
    consume(tokens, (Token::Type)')');
    return new ClassInit(class_name, std::move(args));
}

Expression *Parser::parse_term(vector<Token> &tokens) {
    Token token;
    Expression *exp;
    if (tokens[_pos].type == Token::KW_NULL) {
        _pos++;
        return new NullExp();
    }
    if (tokens[_pos].type == Token::TK_RAWSTRING) {
        string s = tokens[_pos].id;
        _pos++;
        return new StringExp(s);
    }
    if ((exp = parse_class_init(tokens))) return exp;
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

