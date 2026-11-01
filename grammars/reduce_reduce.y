%token ID
%%
item
    : decl ';'
    | expr ';'
    ;
decl
    : type_name ID
    | type_name
    ;
expr
    : var
    ;
type_name
    : ID
    ;
var
    : ID
    ;
