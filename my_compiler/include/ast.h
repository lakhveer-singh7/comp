#pragma once
#include <memory>
#include <string>
#include <vector>

struct Expr {
    virtual ~Expr() = default;
};

struct NumberExpr : Expr {
    long value;
    explicit NumberExpr(long v) : value(v) {}
};

struct VarExpr : Expr {
    std::string name;
    explicit VarExpr(std::string n) : name(std::move(n)) {}
};

struct BinaryExpr : Expr {
    char op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    BinaryExpr(char o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct Stmt { virtual ~Stmt() = default; };

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value;
    explicit ReturnStmt(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

struct Function {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<Stmt>> body;
};
