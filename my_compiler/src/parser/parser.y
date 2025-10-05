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

%define api.value.type {int}

%token T_INT T_RETURN T_IF T_ELSE T_WHILE T_FOR T_DO T_SWITCH T_CASE T_DEFAULT T_BREAK T_CONTINUE T_GOTO T_TYPEDEF T_STATIC
%token T_ID T_NUM
%token T_EQ T_NE T_LE T_GE T_AND T_OR

%left T_OR
%left T_AND
%left T_EQ T_NE
%left '<' '>' T_LE T_GE
%left '+' '-'
%left '*' '/'

%%
translation_unit
  : function                               { /* result in g_resultFunction */ }
  ;

function
  : T_INT T_ID '(' ')' '{' T_RETURN expr ';' '}'
    {
      auto fn = std::make_unique<Function>();
      fn->name = "main"; /* use parsed name if needed */
      fn->body.emplace_back(std::make_unique<ReturnStmt>($7 ? std::make_unique<NumberExpr>($7) : std::make_unique<NumberExpr>(0)));
      g_resultFunction = std::move(fn);
    }
  ;

expr
  : expr '+' expr   { $$ = $1 + $3; }
  | expr '-' expr   { $$ = $1 - $3; }
  | expr '*' expr   { $$ = $1 * $3; }
  | expr '/' expr   { $$ = $3 ? $1 / $3 : 0; }
  | '(' expr ')'    { $$ = $2; }
  | T_NUM           { $$ = $1; }
  ;
%%

void yyerror(const char* s) {
  fprintf(stderr, "parse error: %s\n", s);
}
