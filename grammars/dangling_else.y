%token IF ELSE COND OTHER
%%
stmt
    : IF '(' COND ')' stmt
    | IF '(' COND ')' stmt ELSE stmt
    | OTHER ';'
    ;
