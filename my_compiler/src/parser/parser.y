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
  Stmt* stmt;
  BlockStmt* block;
  std::vector<Expr*>* args;
  std::vector<Stmt*>* slist;
  std::vector<SwitchCase>* cases;
}

%token T_INT T_CHAR T_VOID T_STRUCT T_TYPEDEF T_STATIC
%token T_RETURN T_IF T_ELSE T_WHILE T_FOR T_DO T_SWITCH T_CASE T_DEFAULT T_BREAK T_CONTINUE T_GOTO
%token <sval> T_ID
%token <ival> T_NUM
%token T_EQ T_NE T_LE T_GE T_AND T_OR T_SHL T_SHR T_ARROW T_STRING

%left T_OR
%left T_AND
%left T_EQ T_NE
%left '<' '>' T_LE T_GE
%left '+' '-'
%left '*' '/' '%'
%right UMINUS '!' '~'
%type <expr> expr assignment logical_or logical_and equality relational additive multiplicative unary postfix primary opt_expr
%type <stmt> stmt expr_stmt selection_stmt iteration_stmt jump_stmt compound_stmt declaration label_stmt switch_stmt
%type <args> arg_list
%type <slist> stmt_list default_block_opt
%type <cases> case_blocks

%%
translation_unit
  : function                               { /* result in g_resultFunction */ }
  ;

function
  : T_INT T_ID '(' ')' compound_stmt
    {
      auto fn = std::make_unique<Function>();
      fn->name = std::string($2);
      free($2);
      fn->bodyBlock.reset(static_cast<BlockStmt*>($5));
      g_resultFunction = std::move(fn);
    }
  ;

compound_stmt
  : '{' '}'
    {
      auto blk = new BlockStmt();
      $$ = reinterpret_cast<Stmt*>(blk);
    }
  | '{' stmt '}'
    {
      auto blk = new BlockStmt();
      blk->statements.emplace_back(std::unique_ptr<Stmt>($2));
      $$ = reinterpret_cast<Stmt*>(blk);
    }
  | '{' stmt stmt '}'
    {
      auto blk = new BlockStmt();
      blk->statements.emplace_back(std::unique_ptr<Stmt>($2));
      blk->statements.emplace_back(std::unique_ptr<Stmt>($3));
      $$ = reinterpret_cast<Stmt*>(blk);
    }
  ;

stmt
  : expr_stmt
  | selection_stmt
  | iteration_stmt
  | jump_stmt
  | compound_stmt
  | declaration
  | label_stmt
  ;

expr_stmt
  : expr ';'        { $$ = new ExprStmt(std::unique_ptr<Expr>($1)); }
  | ';'             { $$ = new ExprStmt(std::unique_ptr<Expr>()); }
  ;

selection_stmt
  : T_IF '(' expr ')' stmt
    {
      auto node = new IfStmt();
      node->condition.reset($3);
      node->thenBranch.reset($5);
      $$ = node;
    }
  | T_IF '(' expr ')' stmt T_ELSE stmt
    {
      auto node = new IfStmt();
      node->condition.reset($3);
      node->thenBranch.reset($5);
      node->elseBranch.reset($7);
      $$ = node;
    }
  ;

iteration_stmt
  : T_WHILE '(' expr ')' stmt
    {
      auto node = new WhileStmt();
      node->condition.reset($3);
      node->body.reset($5);
      $$ = node;
    }
  | T_DO stmt T_WHILE '(' expr ')' ';'
    {
      auto node = new DoWhileStmt();
      node->body.reset($2);
      node->condition.reset($5);
      $$ = node;
    }
  ;

  | T_FOR '(' opt_expr ';' opt_expr ';' opt_expr ')' stmt
    {
      auto fs = new ForStmt();
      if ($3) fs->init.reset(new ExprStmt(std::unique_ptr<Expr>($3)));
      if ($5) fs->condition.reset($5);
      if ($7) fs->iter.reset(new ExprStmt(std::unique_ptr<Expr>($7)));
      fs->body.reset($9);
      $$ = fs;
    }
  ;
opt_expr
  : expr { $$ = $1; }
  | /* empty */ { $$ = nullptr; }
  ;

label_stmt
  : T_ID ':' stmt
    {
      auto blk = new BlockStmt();
      auto lab = new LabelStmt(); lab->label = std::string($1); free($1);
      blk->statements.emplace_back(lab);
      blk->statements.emplace_back($3);
      $$ = blk;
    }
  ;

declaration
  : T_STATIC T_INT T_ID ';'
    {
      auto d = new VarDeclStmt(); d->isStatic = true; d->name = std::string($3); free($3); d->type = Type::Int(); $$ = d;
    }
  | T_INT T_ID ';'
    {
      auto d = new VarDeclStmt(); d->isStatic = false; d->name = std::string($2); free($2); d->type = Type::Int(); $$ = d;
    }
  | T_INT T_ID '=' expr ';'
    {
      auto d = new VarDeclStmt(); d->isStatic = false; d->name = std::string($2); free($2); d->type = Type::Int(); d->init.reset($4); $$ = d;
    }
  ;

switch_stmt
  : T_SWITCH '(' expr ')' '{' case_blocks default_block_opt '}'
    {
      auto sw = new SwitchStmt(); sw->value.reset($3);
      for (auto &c : *$6) sw->cases.push_back(c);
      delete $6;
      for (auto *s : *$7) sw->defaultBody.emplace_back(s);
      delete $7;
      $$ = sw;
    }
  ;

case_blocks
  : case_blocks T_CASE T_NUM ':' stmt_list
    {
      $$ = $1;
      SwitchCase c; c.value = $3; for (auto* s : *$5) c.statements.emplace_back(s); delete $5; $$.push_back(std::move(c));
    }
  | /* empty */
    {
      $$ = new std::vector<SwitchCase>();
    }
  ;

stmt_list
  : stmt_list stmt { $1->push_back($2); $$ = $1; }
  | /* empty */    { $$ = new std::vector<Stmt*>(); }
  ;

default_block_opt
  : T_DEFAULT ':' stmt_list { $$ = $3; }
  | /* empty */             { $$ = new std::vector<Stmt*>(); }
  ;

jump_stmt
  : T_RETURN expr ';'   { $$ = new ReturnStmt(std::unique_ptr<Expr>($2)); }
  | T_BREAK ';'         { $$ = new BreakStmt(); }
  | T_CONTINUE ';'      { $$ = new ContinueStmt(); }
  | T_GOTO T_ID ';'     { auto n=new GotoStmt(); n->label=std::string($2); free($2); $$=n; }
  ;

expr
  : assignment { $$ = $1; }
  ;

assignment
  : logical_or '=' assignment { $$ = new AssignExpr(std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | logical_or               { $$ = $1; }
  ;

logical_or
  : logical_or T_OR logical_and { $$ = new BinaryExpr("||", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | logical_and                 { $$ = $1; }
  ;

logical_and
  : logical_and T_AND equality  { $$ = new BinaryExpr("&&", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | equality                    { $$ = $1; }
  ;

equality
  : equality T_EQ relational   { $$ = new BinaryExpr("==", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | equality T_NE relational   { $$ = new BinaryExpr("!=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | relational                 { $$ = $1; }
  ;

relational
  : relational '<' additive    { $$ = new BinaryExpr("<", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | relational '>' additive    { $$ = new BinaryExpr(">", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | relational T_LE additive   { $$ = new BinaryExpr("<=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | relational T_GE additive   { $$ = new BinaryExpr(">=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | additive                   { $$ = $1; }
  ;

additive
  : additive '+' multiplicative { $$ = new BinaryExpr('+', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | additive '-' multiplicative { $$ = new BinaryExpr('-', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | multiplicative              { $$ = $1; }
  ;

multiplicative
  : multiplicative '*' unary { $$ = new BinaryExpr('*', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | multiplicative '/' unary { $$ = new BinaryExpr('/', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | multiplicative '%' unary { $$ = new BinaryExpr('%', std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3)); }
  | unary                    { $$ = $1; }
  ;

unary
  : '-' unary %prec UMINUS   { $$ = new UnaryExpr("-", std::unique_ptr<Expr>($2)); }
  | '!' unary                { $$ = new UnaryExpr("!", std::unique_ptr<Expr>($2)); }
  | postfix                  { $$ = $1; }
  ;

postfix
  : postfix '(' ')'          { auto c=new CallExpr(); c->callee.reset($1); $$ = c; }
  | postfix '(' arg_list ')' { auto c=new CallExpr(); c->callee.reset($1); for(auto* e:*$3){ c->args.emplace_back(std::unique_ptr<Expr>(e)); } delete $3; $$=c; }
  | postfix '[' expr ']'     { auto a=new ArrayIndexExpr(); a->base.reset($1); a->index.reset($3); $$=a; }
  | postfix '.' T_ID         { auto m=new MemberExpr(); m->base.reset($1); m->field=std::string($3); free($3); $$=m; }
  | postfix T_ARROW T_ID     { auto m=new PtrMemberExpr(); m->base.reset($1); m->field=std::string($3); free($3); $$=m; }
  | primary                  { $$ = $1; }
  ;

arg_list
  : expr                     { $$ = new std::vector<Expr*>(); $$->push_back($1); }
  | arg_list ',' expr        { $1->push_back($3); $$ = $1; }
  ;

primary
  : '(' expr ')'             { $$ = $2; }
  | T_NUM                    { $$ = new NumberExpr($1); }
  | T_ID                     { $$ = new VarExpr(std::string($1)); free($1); }
  | T_STRING                 { $$ = new StringLiteralExpr(std::string($1)); free($1); }
  ;
%%

void yyerror(const char* s) {
  fprintf(stderr, "parse error: %s\n", s);
}
