%{
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include "ast.h"

// Interface to the outside world
extern int yylex(void);
extern FILE* yyin;
void yyerror(const char* s);

// Build result
std::unique_ptr<Function> g_resultFunction;
%}

%union {
  long ival;
  char* sval;
  Expr* expr;
}

%token T_INT T_RETURN T_IF T_ELSE T_WHILE T_FOR T_DO T_SWITCH T_CASE T_DEFAULT T_BREAK T_CONTINUE T_GOTO T_TYPEDEF T_STATIC
%token <sval> T_ID
%token <ival> T_NUM
%token T_EQ T_NE T_LE T_GE T_AND T_OR

%left T_OR
%left T_AND
%left T_EQ T_NE
%left '<' '>' T_LE T_GE
%left '+' '-'
%left '*' '/'
%type <expr> expr

%%
translation_unit
  : function                               { /* result in g_resultFunction */ }
  ;

function
  : T_INT T_ID '(' ')' '{' T_RETURN expr ';' '}'
    {
      auto fn = std::make_unique<Function>();
      fn->name = std::string($2);
      free($2);
      fn->body.emplace_back(std::make_unique<ReturnStmt>(std::unique_ptr<Expr>($7)));
      g_resultFunction = std::move(fn);
    }
  ;

expr
  : expr '+' expr   { $$ = new BinaryExpr('+', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | expr '-' expr   { $$ = new BinaryExpr('-', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | expr '*' expr   { $$ = new BinaryExpr('*', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | expr '/' expr   { $$ = new BinaryExpr('/', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | '(' expr ')'    { $$ = $2; }
  | T_NUM           { $$ = new NumberExpr($1); }
  ;
%%

void yyerror(const char* s) {
  fprintf(stderr, "parse error: %s\n", s);
}
