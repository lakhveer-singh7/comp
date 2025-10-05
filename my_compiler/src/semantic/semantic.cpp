#include "ast.h"
#include "symbol_table.h"
#include "type_system.h"
#include "error_handler.h"
#include <unordered_set>
#include <unordered_map>

namespace {
struct Scope {
    std::unordered_set<std::string> vars;
};

struct Ctx {
    std::vector<Scope> scopes;
    int loopDepth = 0;
    ErrorHandler* err = nullptr;

    void push() { scopes.emplace_back(); }
    void pop() { if (!scopes.empty()) scopes.pop_back(); }
    bool declare(const std::string& n) { if (scopes.empty()) push(); if (scopes.back().vars.count(n)) return false; scopes.back().vars.insert(n); return true; }
    bool exists(const std::string& n) const { for (auto it=scopes.rbegin(); it!=scopes.rend(); ++it) if (it->vars.count(n)) return true; return false; }
};

void checkExpr(const Expr* e, Ctx& ctx) {
    if (!e) return;
    if (auto v = dynamic_cast<const VarExpr*>(e)) {
        if (!ctx.exists(v->name)) ctx.err->report({0,0}, "use of undeclared identifier '" + v->name + "'");
    } else if (auto b = dynamic_cast<const BinaryExpr*>(e)) {
        checkExpr(b->lhs.get(), ctx);
        checkExpr(b->rhs.get(), ctx);
    } else if (auto u = dynamic_cast<const UnaryExpr*>(e)) {
        checkExpr(u->operand.get(), ctx);
    } else if (auto a = dynamic_cast<const AssignExpr*>(e)) {
        if (!dynamic_cast<const VarExpr*>(a->target.get()) && !dynamic_cast<const ArrayIndexExpr*>(a->target.get()) && !dynamic_cast<const MemberExpr*>(a->target.get())) {
            ctx.err->report({0,0}, "assignment target is not an lvalue");
        }
        checkExpr(a->target.get(), ctx);
        checkExpr(a->value.get(), ctx);
    } else if (auto c = dynamic_cast<const CallExpr*>(e)) {
        for (auto& arg : c->args) checkExpr(arg.get(), ctx);
    } else if (auto idx = dynamic_cast<const ArrayIndexExpr*>(e)) {
        checkExpr(idx->base.get(), ctx);
        checkExpr(idx->index.get(), ctx);
    } else if (auto m = dynamic_cast<const MemberExpr*>(e)) {
        checkExpr(m->base.get(), ctx);
    } else if (auto pm = dynamic_cast<const PtrMemberExpr*>(e)) {
        checkExpr(pm->base.get(), ctx);
    } else if (auto s = dynamic_cast<const StringLiteralExpr*>(e)) {
        (void)s; // ok
    }
}

void checkStmt(const Stmt* s, Ctx& ctx) {
    if (auto r = dynamic_cast<const ReturnStmt*>(s)) {
        checkExpr(r->value.get(), ctx);
    } else if (auto e = dynamic_cast<const ExprStmt*>(s)) {
        checkExpr(e->expr.get(), ctx);
    } else if (auto b = dynamic_cast<const BlockStmt*>(s)) {
        ctx.push();
        for (const auto& st : b->statements) checkStmt(st.get(), ctx);
        ctx.pop();
    } else if (auto i = dynamic_cast<const IfStmt*>(s)) {
        checkExpr(i->condition.get(), ctx);
        checkStmt(i->thenBranch.get(), ctx);
        if (i->elseBranch) checkStmt(i->elseBranch.get(), ctx);
    } else if (auto w = dynamic_cast<const WhileStmt*>(s)) {
        checkExpr(w->condition.get(), ctx);
        ctx.loopDepth++; checkStmt(w->body.get(), ctx); ctx.loopDepth--;
    } else if (auto d = dynamic_cast<const DoWhileStmt*>(s)) {
        ctx.loopDepth++; checkStmt(d->body.get(), ctx); ctx.loopDepth--;
        checkExpr(d->condition.get(), ctx);
    } else if (auto f = dynamic_cast<const ForStmt*>(s)) {
        ctx.push();
        if (f->init) checkStmt(f->init.get(), ctx);
        if (f->condition) checkExpr(f->condition.get(), ctx);
        ctx.loopDepth++; if (f->body) checkStmt(f->body.get(), ctx); ctx.loopDepth--;
        if (f->iter) checkStmt(f->iter.get(), ctx);
        ctx.pop();
    } else if (auto sw = dynamic_cast<const SwitchStmt*>(s)) {
        checkExpr(sw->value.get(), ctx);
        ctx.loopDepth++; // allow break
        for (const auto& c : sw->cases) { for (const auto& st : c.statements) checkStmt(st.get(), ctx); }
        for (const auto& st : sw->defaultBody) checkStmt(st.get(), ctx);
        ctx.loopDepth--;
    } else if (dynamic_cast<const BreakStmt*>(s) || dynamic_cast<const ContinueStmt*>(s)) {
        if (ctx.loopDepth <= 0) ctx.err->report({0,0}, "break/continue not in loop");
    } else if (auto dcl = dynamic_cast<const VarDeclStmt*>(s)) {
        if (!ctx.declare(dcl->name)) ctx.err->report({0,0}, "redeclaration of '" + dcl->name + "'");
        if (dcl->init) checkExpr(dcl->init.get(), ctx);
    }
}
}

void semanticCheck(const Function& fn, ErrorHandler& err) {
    Ctx ctx; ctx.err = &err; ctx.push();
    // Declare parameters in scope
    for (const auto& p : fn.detailedParams) ctx.declare(p.name);
    if (fn.bodyBlock) {
        checkStmt(fn.bodyBlock.get(), ctx);
    } else {
        for (const auto& st : fn.body) checkStmt(st.get(), ctx);
    }
}

// Simple module-level checker scaffold: validates duplicate function names and arity match across calls
void semanticCheckModule(const std::vector<std::unique_ptr<Function>>& fns, ErrorHandler& err) {
    std::unordered_map<std::string, const Function*> ftable;
    for (const auto& fn : fns) {
        if (ftable.count(fn->name)) {
            err.report({0,0}, "duplicate function '" + fn->name + "'");
        } else {
            ftable[fn->name] = fn.get();
        }
        // per-function checks
        semanticCheck(*fn, err);
    }
    // Further checks would require expression traversal with types; stubbed for now
}
