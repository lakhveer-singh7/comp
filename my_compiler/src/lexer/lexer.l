%{
#include "lexer.h"
%}

%%
[ \t\r\n]+           ;
"int"              return 0; /* stub */
.                  ;
%%
