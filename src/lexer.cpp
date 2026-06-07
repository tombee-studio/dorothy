/* Copyright 2022(Tomoya Bansho@tomoya-kwansei) */
#include "../include/lexer.hpp"

vector<Token> Lexer::lex(string p) {
    int pos = 0;
    tokenize(p, &pos);
    tokens.push_back(Token::make_operator(Token::TK_EOF));
    return tokens;
}

void Lexer::tokenize(string p, int* ppos) {
    while (p.size() > *ppos) {
        if (skip(p, ppos)) continue;
        if (tokenize_keyword(p, ppos, "if", Token::KW_IF)) continue;
        if (tokenize_keyword(p, ppos, "else", Token::KW_ELSE)) continue;
        if (tokenize_keyword(p, ppos, "var", Token::KW_VAR)) continue;
        if (tokenize_keyword(p, ppos, "let", Token::KW_LET)) continue;
        if (tokenize_keyword(p, ppos, "double", Token::KW_DOUBLE)) continue;
        if (tokenize_keyword(p, ppos, "float", Token::KW_FLOAT)) continue;
        if (tokenize_keyword(p, ppos, "int", Token::KW_INT)) continue;
        if (tokenize_keyword(p, ppos, "long", Token::KW_LONG)) continue;
        if (tokenize_keyword(p, ppos, "char", Token::KW_CHAR)) continue;
        if (tokenize_keyword(p, ppos, "while", Token::KW_WHILE)) continue;
        if (tokenize_keyword(p, ppos, "return", Token::KW_RETURN)) continue;
        if (tokenize_keyword(p, ppos, "func", Token::KW_FUNC)) continue;
        if (tokenize_keyword(p, ppos, "for", Token::KW_FOR)) continue;
        if (tokenize_keyword(p, ppos, "import", Token::KW_IMPORT)) continue;
        if (tokenize_keyword(p, ppos, "<=", Token::TK_LE)) continue;
        if (tokenize_keyword(p, ppos, ">=", Token::TK_GE)) continue;
        if (tokenize_keyword(p, ppos, "==", Token::TK_EQ)) continue;
        if (tokenize_keyword(p, ppos, "!=", Token::TK_NE)) continue;
        if (tokenize_operator(p, ppos, '&')) continue;
        if (tokenize_operator(p, ppos, ';')) continue;
        if (tokenize_operator(p, ppos, '=')) continue;
        if (tokenize_operator(p, ppos, ',')) continue;
        if (tokenize_operator(p, ppos, '+')) continue;
        if (tokenize_operator(p, ppos, '-')) continue;
        if (tokenize_operator(p, ppos, '*')) continue;
        if (tokenize_operator(p, ppos, '/')) continue;
        if (tokenize_operator(p, ppos, '%')) continue;
        if (tokenize_operator(p, ppos, '>')) continue;
        if (tokenize_operator(p, ppos, '<')) continue;
        if (tokenize_operator(p, ppos, '{')) continue;
        if (tokenize_operator(p, ppos, '}')) continue;
        if (tokenize_operator(p, ppos, '(')) continue;
        if (tokenize_operator(p, ppos, ')')) continue;
        if (tokenize_operator(p, ppos, '[')) continue;
        if (tokenize_operator(p, ppos, ']')) continue;
        if (tokenize_operator(p, ppos, ':')) continue;
        if (tokenize_str(p, ppos)) continue;
        if (tokenize_char(p, ppos)) continue;
        if (tokenize_id(p, ppos)) continue;
        if (tokenize_float(p, ppos)) continue;
        if (tokenize_int(p, ppos)) continue;
        throw LexerError(format("undefined token: %c", p[*ppos]).data());
    }
}

bool Lexer::skip(string p, int* ppos) {
    if (p[*ppos] == ' ' || p[*ppos] == '\n') {
        (*ppos)++;
        return true;
    }
    return false;
}

bool Lexer::tokenize_int(string p, int* ppos) {
    int start = *ppos;
    if (p[start] < '0' || p[start] > '9') return false;
    while (p[*ppos] >= '0' && p[*ppos] <= '9') (*ppos)++;
    string str = p.substr(start, *ppos - start);
    tokens.push_back(Token::make_int(stoi(str)));
    return true;
}

bool Lexer::tokenize_char(string p, int* ppos) {
    if (p[*ppos] != '\'') return false;
    (*ppos)++;
    tokens.push_back(Token::make_int(p[*ppos]));
    (*ppos)++;
    if (p[*ppos] != '\'') exit(-1);
    (*ppos)++;
    return true;
}

bool Lexer::tokenize_str(string p, int* ppos) {
    if (p[*ppos] != '\"') return false;
    (*ppos)++;
    tokens.push_back(Token::make_operator((Token::Type)'{'));
    while (true) {
        if (p[*ppos] == '\"') {
            break;
        }
        tokens.push_back(Token::make_int(p[*ppos]));
        tokens.push_back(Token::make_operator((Token::Type)','));
        (*ppos)++;
    }
    tokens.push_back(Token::make_int(0));
    tokens.push_back(Token::make_operator((Token::Type)'}'));
    (*ppos)++;
    return true;
}

bool Lexer::tokenize_float(string p, int* ppos) {
    int start = *ppos;
    if (p[start] < '0' || p[start] > '9') return false;
    int i = start;
    while (p[i] >= '0' && p[i] <= '9') i++;
    if (p[i] != '.') return false;
    i++;
    if (p[i] < '0' || p[i] > '9') return false;
    while (p[i] >= '0' && p[i] <= '9') i++;
    string str = p.substr(start, i - start);
    *ppos = i;
    tokens.push_back(Token::make_float(stod(str)));
    return true;
}

bool Lexer::tokenize_id(string p, int* ppos) {
    int start = *ppos;
    if (p[start] < 'A' || p[start] > 'z' || p[start] == '_') return false;
    while ((p[*ppos] >= 'A' && p[*ppos] <= 'Z') ||
           (p[*ppos] >= 'a' && p[*ppos] <= 'z') || p[*ppos] == '_')
        (*ppos)++;
    string str = p.substr(start, *ppos - start);
    tokens.push_back(Token::make_id(str));
    return true;
}

bool Lexer::tokenize_keyword(string p, int* ppos, string keyword,
                             Token::Type type) {
    int nextIndex = (*ppos) + keyword.size();
    if (p.substr(*ppos, keyword.size()) != keyword) {
        return false;
    }
    // Reject if immediately followed by a letter or underscore (part of an identifier)
    char next = p[nextIndex];
    bool is_id_char = (next >= 'A' && next <= 'Z') ||
                      (next >= 'a' && next <= 'z') ||
                      next == '_';
    if (is_id_char) return false;
    (*ppos) += keyword.size();
    tokens.push_back(Token::make_operator(type));
    return true;
}

bool Lexer::tokenize_operator(string p, int* ppos, char c) {
    if (p[*ppos] == c) {
        (*ppos)++;
        tokens.push_back(Token::make_operator((Token::Type)c));
        return true;
    } else
        return false;
}
