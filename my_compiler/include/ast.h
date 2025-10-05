#pragma once
#include <memory>
#include <string>
#include <vector>
#include "type_system.h"

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

struct UnaryExpr : Expr {
    std::string op; // "-", "!", "&", "*"
    std::unique_ptr<Expr> operand;
    explicit UnaryExpr(std::string o, std::unique_ptr<Expr> e)
        : op(std::move(o)), operand(std::move(e)) {}
};

struct BinaryExpr : Expr {
    std::string op; // "+", "-", "*", "/", "<", ">", "<=", ">=", "==", "!=", "&&", "||"
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    BinaryExpr(char o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op(std::string(1, o)), lhs(std::move(l)), rhs(std::move(r)) {}
    BinaryExpr(std::string o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op(std::move(o)), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct AssignExpr : Expr {
    std::unique_ptr<Expr> target; // must be an lvalue expr (VarExpr, Deref, ArrayIndex, Member)
    std::unique_ptr<Expr> value;
    AssignExpr(std::unique_ptr<Expr> t, std::unique_ptr<Expr> v)
        : target(std::move(t)), value(std::move(v)) {}
};

struct CallExpr : Expr {
    std::unique_ptr<Expr> callee; // VarExpr for named function or expression for fn pointer
    std::vector<std::unique_ptr<Expr>> args;
};

struct ArrayIndexExpr : Expr {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> index;
};

struct MemberExpr : Expr { // base.field
    std::unique_ptr<Expr> base;
    std::string field;
};

struct PtrMemberExpr : Expr { // base->field
    std::unique_ptr<Expr> base;
    std::string field;
};

struct Stmt { virtual ~Stmt() = default; };

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
    explicit ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
};

struct VarDeclStmt : Stmt {
    std::string name;
    Type type;
    bool isStatic = false;
    std::unique_ptr<Expr> init; // optional
};

struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch; // optional
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
};

struct DoWhileStmt : Stmt {
    std::unique_ptr<Stmt> body;
    std::unique_ptr<Expr> condition;
};

struct ForStmt : Stmt {
    std::unique_ptr<Stmt> init;       // optional
    std::unique_ptr<Expr> condition;  // optional
    std::unique_ptr<Stmt> iter;       // optional
    std::unique_ptr<Stmt> body;
};

struct SwitchCase {
    long value;
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct SwitchStmt : Stmt {
    std::unique_ptr<Expr> value;
    std::vector<SwitchCase> cases;
    std::vector<std::unique_ptr<Stmt>> defaultBody; // optional
};

struct BreakStmt : Stmt {};
struct ContinueStmt : Stmt {};
struct GotoStmt : Stmt { std::string label; };
struct LabelStmt : Stmt { std::string label; };

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value;
    explicit ReturnStmt(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

struct FunctionParam { std::string name; Type type; };

struct Function {
    std::string name;
    Type returnType = Type::Int();
    std::vector<std::string> params; // legacy
    std::vector<FunctionParam> detailedParams; // preferred
    std::unique_ptr<BlockStmt> bodyBlock; // preferred body
    std::vector<std::unique_ptr<Stmt>> body; // legacy body
};
