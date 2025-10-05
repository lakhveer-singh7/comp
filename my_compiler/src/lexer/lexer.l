%option noyywrap
%option nounput noinput noyy_top_state
%{
#include <cstdio>
#include "parser.tab.hh"
%}

%option yylineno

%{
#include <cstdlib>
#include <cstring>
#include "parser.tab.hh"
extern YYSTYPE yylval;
%}

digit      [0-9]
floatconst ([0-9]+\.[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?
id         [a-zA-Z_][a-zA-Z0-9_]*
ws         [ \t\r\f\v]

%%
{ws}+                  ;
\n                     ;
"int"                  return T_INT;
"float"                return T_FLOAT;
"return"               return T_RETURN;
"char"                 return T_CHAR;
"void"                 return T_VOID;
"struct"               return T_STRUCT;
"typedef"              return T_TYPEDEF;
"static"               return T_STATIC;
"extern"               return T_EXTERN;
"sizeof"               return T_SIZEOF;
"if"                   return T_IF;
"else"                 return T_ELSE;
"while"                return T_WHILE;
"for"                  return T_FOR;
"do"                   return T_DO;
"switch"               return T_SWITCH;
"case"                 return T_CASE;
"default"              return T_DEFAULT;
"break"                return T_BREAK;
"continue"             return T_CONTINUE;
"goto"                 return T_GOTO;
"typedef"              return T_TYPEDEF;
"static"               return T_STATIC;
// C/C++ style comments
"//"[^\n]*            ;
"/*"([^*]|\*+[^/])*"*/"  ;

"=="                   return T_EQ;
"!="                   return T_NE;
"<="                   return T_LE;
">="                   return T_GE;
"&&"                   return T_AND;
"||"                   return T_OR;
"<<"                   return T_SHL;
">>"                   return T_SHR;

"<"                    return '<';
">"                    return '>';
"+"                    return '+';
"-"                    return '-';
"*"                    return '*';
"/"                    return '/';
"!"                    return '!';
"&"                    return '&';
"|"                    return '|';
"^"                    return '^';
"%"                    return '%';
"."                    return '.';
"->"                   return T_ARROW;
"="                    return '=';
";"                    return ';';
","                    return ',';
"("                    return '(';
")"                    return ')';
"{"                    return '{';
"}"                    return '}';
"["                    return '[';
"]"                    return ']';
":"                    return ':';

{floatconst}           { yylval.sval = strndup(yytext, yyleng); return T_FLOATLIT; }
{digit}+               { yylval.ival = strtol(yytext, NULL, 10); return T_NUM; }
\"([^\\\n]|\\.)*\"   { yylval.sval = strndup(yytext+1, yyleng-2); return T_STRING; }
\'([^\\\n]|\\.)\'     { yylval.ival = (unsigned char)yytext[1]; return T_NUM; }
{id}                    { yylval.sval = strdup(yytext); return T_ID; }
.                       { return yytext[0]; }
%%
