#include "../include/typechecker.hpp"
#include <set>

namespace {
bool reference(VarType t) {
    return t == VarType::CLASS || t == VarType::ARRAY || t == VarType::STRING;
}
TypeInfo type(VarType t, string name = "", bool nullable = false) {
    return TypeInfo{t, name, nullable, {}};
}
TypeInfo declared(const DeclVar *d) {
    auto t = d->getTypeInfo();
    t.base_type = d->getType();
    if (t.type_name.empty()) t.type_name = d->getStructName();
    t.is_nullable = d->isNullable();
    return t;
}
TypeInfo field_type(const FieldInfo &f) {
    auto t = f.type_info;
    t.base_type = f.type;
    if (t.type_name.empty()) t.type_name = f.struct_name;
    t.is_nullable = f.is_nullable;
    return t;
}
struct Value {
    TypeInfo type;
    bool nonnull = true;
    bool null_literal = false;
};
struct Binding { TypeInfo type; bool nonnull; };
struct Flow {
    map<string, Binding> vars;
    std::set<string> fields; // stable field paths proven non-null since last mutation/call
    std::set<string> initialized;
    bool live = true;
};

class NullChecker {
    map<string, Function *> functions;
    map<DeclVar *, TypeInfo> inferred;
    string context, owner;
    TypeInfo result;
    bool constructor = false, struct_constructor = false;
    std::set<string> required;

    [[noreturn]] void error(const string &message) const {
        throw CompileError(("null safety in " + context + ": " + message).c_str());
    }
    void demand(const Value &v, const string &where) const {
        if (v.null_literal || (reference(v.type.base_type) && !v.nonnull))
            error("possibly null " + where + "; check against null first");
    }
    void collect_required(const vector<FieldInfo> &fields, const string &prefix = "",
                          std::set<string> visiting = {}) {
        for (const auto &field : fields) {
            string name = prefix + field.name;
            if (reference(field.type) && !field.is_nullable) required.insert(name);
            if (field.type == VarType::STRUCT && g_struct_defs.count(field.struct_name)) {
                if (!visiting.insert(field.struct_name).second)
                    error("recursive value-type field layout");
                collect_required(g_struct_defs.at(field.struct_name).fields, name + ".", visiting);
                visiting.erase(field.struct_name);
            }
        }
    }
    void initialized(const Flow &f) const {
        for (auto &name : required)
            if (!f.initialized.count(name)) error("non-null field '" + name + "' is not initialized");
    }
    static string path(const Expression *e) {
        if (dynamic_cast<const ThisExpr *>(e)) return "this";
        if (auto v = dynamic_cast<const Variable *>(e)) return v->getVarName();
        if (auto m = dynamic_cast<const MemberAccess *>(e)) {
            string p = path(m->getObject());
            if (!p.empty()) return p + "." + m->getMember();
        }
        return "";
    }
    void invalidate(Flow &f) { f.fields.clear(); }
    void compatible(const Value &v, const TypeInfo &target, const string &where) const {
        if (!reference(target.base_type)) {
            if (v.null_literal) error("null cannot be used as " + where);
            if (reference(v.type.base_type)) demand(v, where);
            return;
        }
        if (!v.null_literal && !reference(v.type.base_type))
            error("non-reference value cannot be used as " + where + "; use null for nullable references");
        if (!v.null_literal && v.type.base_type != target.base_type)
            error("reference types differ in " + where);
        if (!target.is_nullable) demand(v, where);
        if (target.base_type == VarType::ARRAY && v.type.base_type == VarType::ARRAY &&
            !target.generic_args.empty() && !v.type.generic_args.empty()) {
            // Arrays are mutable: element nullability must be invariant in both directions.
            auto a = target.get_element_type(), b = v.type.get_element_type();
            if (reference(a.base_type) != reference(b.base_type))
                error("array reference element types differ in " + where);
            if (reference(a.base_type) && a.is_nullable != b.is_nullable)
                error("array element nullability differs in " + where);
            compatible(Value{b, !b.is_nullable}, a, where + " array element");
        }
    }
    const FieldInfo *field(const TypeInfo &t, const string &name) const {
        const vector<FieldInfo> *fields = nullptr;
        if (t.base_type == VarType::CLASS && g_class_defs.count(t.type_name))
            fields = &g_class_defs.at(t.type_name).fields;
        if (t.base_type == VarType::STRUCT && g_struct_defs.count(t.type_name))
            fields = &g_struct_defs.at(t.type_name).fields;
        if (fields) for (auto &f : *fields) if (f.name == name) return &f;
        return nullptr;
    }
    Value member(const MemberAccess *m, Flow &f, bool writing) {
        Value obj;
        if (constructor && dynamic_cast<ThisExpr *>(m->getObject()))
            obj = {type(struct_constructor ? VarType::STRUCT : VarType::CLASS, owner), true};
        else if (auto parent = dynamic_cast<MemberAccess *>(m->getObject())) obj = member(parent, f, false);
        else obj = expr(m->getObject(), f);
        demand(obj, "receiver of ." + m->getMember());
        if (obj.type.base_type == VarType::ARRAY && m->getMember() == "length") return {type(VarType::LONG)};
        auto info = field(obj.type, m->getMember());
        if (!info) return {type(VarType::LONG)}; // ordinary type/field diagnostics remain in the backend
        TypeInfo t = field_type(*info);
        string key = path(m);
        string leaf = key.compare(0, 5, "this.") == 0 ? key.substr(5) : "";
        if (!writing && constructor && required.count(leaf) && !f.initialized.count(leaf))
            error("field '" + leaf + "' is read before initialization");
        return {t, !t.is_nullable || (!key.empty() && f.fields.count(key))};
    }
    void arguments(const vector<Expression *> &args, const vector<DeclVar *> &params,
                   Flow &f, const string &callee) {
        if (args.size() != params.size()) error("incorrect argument count for " + callee);
        for (size_t i = 0; i < args.size(); ++i) {
            if (i < params.size()) {
                auto t = declared(params[i]);
                auto v = expr(args[i], f, &t);
                compatible(v, t, "argument '" + params[i]->getId() + "' to " + callee);
            } else expr(args[i], f);
        }
    }
    void ctor_arguments(const vector<Expression *> &args, ConstructorInfo *ctor,
                        Flow &f, const string &callee) {
        if (ctor) arguments(args, ctor->param_decls, f, callee);
        else for (auto arg : args) expr(arg, f);
    }
    Value expr(const Expression *e, Flow &f, const TypeInfo *expected = nullptr) {
        if (!e) return {type(VarType::LONG)};
        if (dynamic_cast<const NullExp *>(e)) return {type(VarType::CLASS, "", true), false, true};
        if (dynamic_cast<const ThisExpr *>(e)) {
            if (constructor) {
                initialized(f);
                if (!struct_constructor) error("constructors cannot expose this or call virtual methods");
            }
            string name = owner;
            if (auto s = dynamic_cast<const SuperExpr *>(e)) name = s->getBaseClass();
            return {type(struct_constructor ? VarType::STRUCT : VarType::CLASS, name)};
        }
        if (auto v = dynamic_cast<const Variable *>(e)) {
            auto it = f.vars.find(v->getVarName());
            if (it != f.vars.end()) return {it->second.type, it->second.nonnull};
            return {type(VarType::LONG)};
        }
        if (auto m = dynamic_cast<const MemberAccess *>(e)) {
            auto value = member(m, f, false);
            string prefix = path(m);
            if (constructor && value.type.base_type == VarType::STRUCT && prefix.compare(0, 5, "this.") == 0) {
                prefix = prefix.substr(5) + ".";
                for (auto &name : required)
                    if (name.compare(0, prefix.size(), prefix) == 0 && !f.initialized.count(name))
                        error("struct field is copied before initialization");
            }
            return value;
        }
        if (auto a = dynamic_cast<const Assign *>(e)) {
            auto lhs = a->getLeftSide();
            TypeInfo target;
            if (auto m = dynamic_cast<MemberAccess *>(lhs)) target = member(m, f, true).type;
            else if (auto index = dynamic_cast<ArrayIndex *>(lhs)) {
                auto arr = expr(index->getPointer(), f);
                demand(arr, "array receiver"); expr(index->getIndex(), f);
                target = arr.type.get_element_type();
            } else if (auto var = dynamic_cast<Variable *>(lhs)) {
                auto it = f.vars.find(var->getVarName());
                if (it != f.vars.end()) target = it->second.type;
            } else expr(lhs, f);
            auto value = expr(a->getExpr(), f, &target);
            compatible(value, target, "assignment target");
            invalidate(f); // includes aliases of mutable field paths
            if (auto var = dynamic_cast<Variable *>(lhs)) {
                auto it = f.vars.find(var->getVarName());
                if (it != f.vars.end()) it->second.nonnull = value.nonnull && !value.null_literal;
            } else if (auto m = dynamic_cast<MemberAccess *>(lhs)) {
                string key = path(m);
                if (constructor && key.compare(0, 5, "this.") == 0) {
                    string name = key.substr(5);
                    f.initialized.insert(name);
                    if (target.base_type == VarType::STRUCT)
                        for (auto &required_name : required)
                            if (required_name.compare(0, name.size() + 1, name + ".") == 0)
                                f.initialized.insert(required_name);
                }
                if (value.nonnull && !value.null_literal && !path(m).empty()) f.fields.insert(path(m));
            }
            return value;
        }
        if (auto c = dynamic_cast<const ClassInit *>(e)) {
            auto it = g_class_defs.find(c->getClassName());
            if (it != g_class_defs.end() && !it->second.constructor && !c->getArgs().empty())
                error("default constructor takes no arguments");
            ctor_arguments(c->getArgs(), it == g_class_defs.end() ? nullptr : it->second.constructor, f, c->getClassName());
            invalidate(f);
            return {type(VarType::CLASS, c->getClassName())};
        }
        if (auto c = dynamic_cast<const StructInit *>(e)) {
            auto it = g_struct_defs.find(c->getStructName());
            ctor_arguments(c->getArgs(), it == g_struct_defs.end() ? nullptr : it->second.constructor, f, c->getStructName());
            invalidate(f);
            return {type(VarType::STRUCT, c->getStructName())};
        }
        if (auto c = dynamic_cast<const SuperConstructorExp *>(e)) {
            const auto &base = g_class_defs.at(c->getBaseClass());
            ctor_arguments(c->getArgs(), base.constructor, f, c->getBaseClass());
            invalidate(f);
            for (auto &field : base.fields) {
                f.initialized.insert(field.name);
                for (auto &name : required)
                    if (name.compare(0, field.name.size() + 1, field.name + ".") == 0) f.initialized.insert(name);
            }
            return {type(VarType::LONG)};
        }
        if (auto c = dynamic_cast<const CallFuncExp *>(e)) {
            auto it = functions.find(c->getId());
            if (it != functions.end() && !it->second->isImport()) {
                arguments(c->getArgs(), it->second->getArgs(), f, c->getId());
                invalidate(f);
                auto fn = it->second;
                TypeInfo t = fn->getReturnTypeInfo();
                t.base_type = fn->getRetType(); t.type_name = fn->getRetStructName(); t.is_nullable = fn->isRetNullable();
                return {t, !t.is_nullable};
            }
            for (auto arg : c->getArgs()) {
                auto v = expr(arg, f);
                if (reference(v.type.base_type)) demand(v, "argument to external function " + c->getId());
            }
            invalidate(f);
            return {type(VarType::LONG)};
        }
        if (auto c = dynamic_cast<const CallMethodExp *>(e)) {
            auto obj = expr(c->getObject(), f);
            demand(obj, "receiver of method " + c->getMethodName());
            if (obj.type.base_type == VarType::ARRAY) {
                auto element = obj.type.get_element_type();
                for (auto arg : c->getArgs()) {
                    auto v = expr(arg, f, c->getMethodName() == "push" ? &element : nullptr);
                    if (c->getMethodName() == "push") compatible(v, element, "array element");
                }
                invalidate(f);
                return {type(VarType::LONG)};
            }
            if (g_class_defs.count(obj.type.type_name)) {
                auto method = g_class_defs.at(obj.type.type_name).getMethod(c->getMethodName());
                if (method) {
                    arguments(c->getArgs(), method->params, f, c->getMethodName());
                    invalidate(f);
                    return {type(method->ret_type, method->ret_type_name, method->is_ret_nullable), !method->is_ret_nullable};
                }
            }
            for (auto arg : c->getArgs()) expr(arg, f);
            invalidate(f);
            return {type(VarType::LONG)};
        }
        if (auto a = dynamic_cast<const ArrayLiteralExp *>(e)) {
            TypeInfo t = expected && expected->base_type == VarType::ARRAY ? *expected : type(VarType::ARRAY, "Array");
            t.is_nullable = false;
            if (!t.generic_args.empty()) {
                for (auto item : a->getElements()) {
                    auto elem = t.get_element_type();
                    compatible(expr(item, f, &elem), elem, "array literal element");
                }
            } else {
                for (auto item : a->getElements()) {
                    auto v = expr(item, f);
                    if (t.generic_args.empty()) t.generic_args.push_back(v.type);
                    if (!v.nonnull || v.null_literal) t.generic_args[0].is_nullable = true;
                }
            }
            if (t.generic_args.empty()) t.generic_args.push_back(type(VarType::LONG));
            return {t};
        }
        if (auto a = dynamic_cast<const ArrayIndex *>(e)) {
            auto array = expr(a->getPointer(), f);
            demand(array, "array receiver"); expr(a->getIndex(), f);
            auto t = array.type.base_type == VarType::ARRAY ? array.type.get_element_type() : type(VarType::LONG);
            return {t, !t.is_nullable};
        }
        auto children = e->children();
        bool comparison = dynamic_cast<const EQExp *>(e) || dynamic_cast<const NEExp *>(e);
        Value left{type(VarType::LONG)};
        for (size_t i = 0; i < children.size(); ++i) {
            auto value = expr(children[i], f);
            if (!comparison && reference(value.type.base_type)) demand(value, "operand");
            if (!comparison && value.null_literal) error("null is only valid as a nullable reference or in a null comparison");
            if (!i) left = value;
        }
        if (dynamic_cast<const StringExp *>(e)) return {type(VarType::STRING)};
        if (dynamic_cast<const AddExp *>(e) && left.type.base_type == VarType::STRING) return left;
        if (dynamic_cast<const FloatExp *>(e)) return {type(VarType::DOUBLE)};
        return {type(VarType::LONG)};
    }
    void refine(const Expression *condition, bool truth, Flow &f) {
        const Expression *a = nullptr, *b = nullptr;
        bool nonnull = truth;
        if (auto c = dynamic_cast<const NEExp *>(condition)) { a = c->getLeft(); b = c->getRight(); }
        if (auto c = dynamic_cast<const EQExp *>(condition)) { a = c->getLeft(); b = c->getRight(); nonnull = !truth; }
        if (!a) return;
        if (dynamic_cast<const NullExp *>(a)) std::swap(a, b);
        if (!dynamic_cast<const NullExp *>(b)) return;
        string key = path(a);
        if (key.empty()) return;
        auto it = f.vars.find(key);
        if (it != f.vars.end()) it->second.nonnull = nonnull;
        else if (nonnull) f.fields.insert(key);
        else f.fields.erase(key);
    }
    static Flow join(const Flow &a, const Flow &b) {
        if (!a.live) return b;
        if (!b.live) return a;
        Flow result = a;
        for (auto &entry : result.vars) {
            auto it = b.vars.find(entry.first);
            entry.second.nonnull = entry.second.nonnull && it != b.vars.end() && it->second.nonnull;
        }
        for (auto it = result.fields.begin(); it != result.fields.end();)
            if (!b.fields.count(*it)) it = result.fields.erase(it); else ++it;
        for (auto it = result.initialized.begin(); it != result.initialized.end();)
            if (!b.initialized.count(*it)) it = result.initialized.erase(it); else ++it;
        return result;
    }
    static bool same(const Flow &a, const Flow &b) {
        if (a.fields != b.fields || a.initialized != b.initialized || a.vars.size() != b.vars.size()) return false;
        for (auto &v : a.vars) if (!b.vars.count(v.first) || v.second.nonnull != b.vars.at(v.first).nonnull) return false;
        return true;
    }
    Flow loop(const Expression *cond, const Statement *body, const Expression *step, Flow entry) {
        Flow head = entry;
        for (;;) {
            Flow iteration = head;
            expr(cond, iteration);
            Flow exit = iteration;
            refine(cond, false, exit);
            refine(cond, true, iteration);
            iteration = stmt(body, iteration);
            if (iteration.live) expr(step, iteration);
            Flow next = join(entry, iteration);
            if (same(head, next)) return exit;
            head = next;
        }
    }
    Flow stmt(const Statement *s, Flow f) {
        if (!s || !f.live) return f;
        if (auto b = dynamic_cast<const Block *>(s)) {
            map<string, Binding> shadowed;
            std::set<string> declarations;
            for (auto child : b->getStatements()) {
                if (!f.live) break;
                if (auto d = dynamic_cast<DeclVarSt *>(child)) {
                    string name = d->getDecl()->getId();
                    if (!declarations.count(name) && f.vars.count(name)) shadowed[name] = f.vars.at(name);
                    declarations.insert(name);
                }
                f = stmt(child, f);
            }
            for (auto &name : declarations) {
                if (shadowed.count(name)) f.vars[name] = shadowed.at(name);
                else f.vars.erase(name);
            }
            // Field proofs involving shadowed names cannot escape their lexical scope.
            for (auto it = f.fields.begin(); it != f.fields.end();) {
                string root = it->substr(0, it->find('.'));
                if (declarations.count(root)) it = f.fields.erase(it); else ++it;
            }
        } else if (auto d = dynamic_cast<const DeclVarSt *>(s)) {
            auto decl = d->getDecl();
            auto t = declared(decl);
            bool nonnull = !reference(t.base_type);
            if (auto init = dynamic_cast<InitializedDeclVar *>(decl)) {
                auto v = expr(init->getInit(), f, t.base_type == VarType::INFERRED ? nullptr : &t);
                if (t.base_type == VarType::INFERRED) {
                    if (v.null_literal) error("cannot infer a type from null; declare an explicit nullable type");
                    t = v.type;
                    t.is_nullable = !v.nonnull;
                    auto previous = inferred.find(decl);
                    if (previous != inferred.end()) t.is_nullable = t.is_nullable || previous->second.is_nullable;
                    inferred[decl] = t;
                }
                compatible(v, t, "variable '" + decl->getId() + "'");
                nonnull = v.nonnull && !v.null_literal;
            } else if (auto array = dynamic_cast<InitializedDeclArrayVar *>(decl)) {
                for (auto value : array->getValues()) compatible(expr(value, f), t, "fixed array element");
            } else if (reference(t.base_type) && !t.is_nullable) {
                error("non-null variable '" + decl->getId() + "' requires an initializer");
            }
            for (auto it = f.fields.begin(); it != f.fields.end();)
                if (it->substr(0, it->find('.')) == decl->getId()) it = f.fields.erase(it); else ++it;
            f.vars[decl->getId()] = {t, nonnull};
        } else if (auto i = dynamic_cast<const IfSt *>(s)) {
            expr(i->getCond(), f);
            auto yes = f, no = f;
            refine(i->getCond(), true, yes); refine(i->getCond(), false, no);
            f = join(stmt(i->getTrueSt(), yes), stmt(i->getFalseSt(), no));
        } else if (auto w = dynamic_cast<const WhileSt *>(s)) {
            f = loop(w->getCond(), w->getBody(), nullptr, f);
        } else if (auto l = dynamic_cast<const ForSt *>(s)) {
            expr(l->getInit(), f);
            f = loop(l->getCond(), l->getBody(), l->getProceed(), f);
        } else if (auto r = dynamic_cast<const ReturnSt *>(s)) {
            auto v = expr(r->getExpr(), f, &result);
            if (constructor) initialized(f);
            else compatible(v, result, "return value");
            f.live = false;
        } else if (auto e = dynamic_cast<const ExpressionSt *>(s)) expr(e->getExpr(), f);
        else if (auto c = dynamic_cast<const CallMethodSt *>(s)) expr(c->getCall(), f);
        else if (auto c = dynamic_cast<const CallFuncSt *>(s)) {
            CallFuncExp call(c->getId(), c->getArgs()); expr(&call, f);
        }
        return f;
    }
    Flow params(const vector<DeclVar *> &parameters) {
        Flow f;
        for (auto p : parameters) {
            auto t = declared(p); f.vars[p->getId()] = {t, !t.is_nullable};
        }
        return f;
    }
    void body(const Statement *body, const vector<DeclVar *> &parameters) {
        Flow f = stmt(body, params(parameters));
        if (constructor && f.live) initialized(f);
        if (!constructor && reference(result.base_type) && !result.is_nullable && f.live)
            error("non-null return value is not provided on every path");
    }
public:
    void check(const vector<Function *> &program) {
        for (auto f : program) functions[f->getName()] = f;
        for (auto &entry : g_class_defs) {
            const auto &c = entry.second;
            owner = c.name; context = owner + ".constructor";
            constructor = true; struct_constructor = false; required.clear();
            collect_required(c.fields);
            if (c.constructor) body(c.constructor->body, c.constructor->param_decls);
            else if (!required.empty()) error("a constructor must initialize non-null reference fields");
            constructor = false; required.clear();
            for (auto &m : c.methods) {
                auto method = m.second;
                if (method->class_name != owner) continue;
                if (!c.base_class.empty()) {
                    auto parent = g_class_defs.at(c.base_class).getMethod(method->name);
                    if (parent) {
                        if (parent->params.size() != method->params.size() ||
                            reference(parent->ret_type) != reference(method->ret_type))
                            error("override must preserve the parent reference signature");
                        if (reference(parent->ret_type) && !parent->is_ret_nullable && method->is_ret_nullable)
                            error("override cannot make a non-null return nullable");
                        for (size_t i = 0; i < parent->params.size(); ++i) {
                            if (reference(parent->params[i]->getType()) != reference(method->params[i]->getType()))
                                error("override must preserve reference parameter types");
                            if (reference(parent->params[i]->getType()) && parent->params[i]->isNullable() && !method->params[i]->isNullable())
                                error("override cannot require a non-null argument accepted as nullable by its parent");
                        }
                    }
                }
                if (method->is_abstract) continue;
                context = owner + "." + method->name;
                result = type(method->ret_type, method->ret_type_name, method->is_ret_nullable);
                body(method->body, method->params);
            }
        }
        for (auto &entry : g_struct_defs) {
            const auto &s = entry.second;
            owner = s.name; context = owner + ".constructor";
            constructor = struct_constructor = true; required.clear();
            collect_required(s.fields);
            if (s.constructor) body(s.constructor->body, s.constructor->param_decls);
            else if (!required.empty()) error("a constructor must initialize non-null reference fields");
        }
        owner.clear(); constructor = struct_constructor = false; required.clear();
        for (auto f : program) {
            if (f->isImport()) continue;
            context = f->getName();
            result = f->getReturnTypeInfo();
            result.base_type = f->getRetType(); result.type_name = f->getRetStructName(); result.is_nullable = f->isRetNullable();
            body(f->getBody(), f->getArgs());
        }
        for (auto &entry : inferred) {
            auto t = entry.second;
            if (reference(t.base_type) || t.base_type == VarType::STRUCT) {
                string name = t.type_name;
                if (t.base_type == VarType::ARRAY) {
                    auto element = t.get_element_type();
                    string element_name = element.type_name;
                    if (element_name.empty()) element_name = element.base_type == VarType::STRING ? "string" : "long";
                    name = "Array<" + element_name + ">";
                }
                entry.first->resolveReferenceType(t, name);
            }
        }
    }
};
} // namespace

void check_null_safety(const vector<Function *> &program) {
    NullChecker().check(program);
}
