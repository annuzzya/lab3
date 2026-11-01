%{
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "diag.h"

int yylex(void);
void yyerror(const char *msg);

extern char *yytext;
Node *parse_result;
%}

%locations
%error-verbose

%union {
    long ival;
    char *str;
    Node *node;
    NodeList *list;
    Type type;
}

%token INT "int" DOUBLE "double" VOID "void"
%token IF "if" ELSE "else" WHILE "while" DO "do" FOR "for"
%token RETURN "return" BREAK "break" CONTINUE "continue"
%token <str> IDENT "identifier" DOUBLE_LIT "floating constant" STRING_LIT "string literal"
%token <ival> INT_LIT "integer constant"
%token INC "++" DEC "--" LE "<=" GE ">=" EQ "==" NE "!=" AND "&&" OR "||"
%token ADD_ASSIGN "+=" SUB_ASSIGN "-=" MUL_ASSIGN "*=" DIV_ASSIGN "/=" MOD_ASSIGN "%="

%nonassoc THEN
%nonassoc ELSE

%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%right '?' ':'
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right UNARY
%left INC DEC '['

%type <type> type_spec
%type <node> external_decl function_def declaration init_declarator param
%type <node> statement compound_stmt block_item for_init expr opt_expr primary
%type <list> translation_unit init_declarator_list params param_list block_items args opt_args

%destructor { free($$); } IDENT DOUBLE_LIT STRING_LIT
%destructor { ast_free($$); } external_decl function_def declaration init_declarator param
%destructor { ast_free($$); } statement compound_stmt block_item for_init expr opt_expr primary
%destructor { list_free($$); } translation_unit init_declarator_list params param_list block_items args opt_args

%start program
%expect 0

%%

program
    : translation_unit                      { parse_result = mk_program($1); }
    ;

translation_unit
    : /* empty */                           { $$ = list_new(); }
    | translation_unit external_decl        { $$ = $2 ? list_push($1, $2) : $1; }
    ;

external_decl
    : function_def
    | declaration
    | error ';'                             { $$ = NULL; yyerrok; }
    | error '}'                             { $$ = NULL; yyerrok; }
    ;

type_spec
    : INT                                   { $$ = T_INT; }
    | DOUBLE                                { $$ = T_DOUBLE; }
    | VOID                                  { $$ = T_VOID; }
    ;

function_def
    : type_spec IDENT '(' params ')' compound_stmt
                                            { $$ = mk_func($1, $2, $4, $6, @2.first_line); }
    | type_spec IDENT '(' params ')' ';'    { $$ = mk_func($1, $2, $4, NULL, @2.first_line); }
    ;

params
    : /* empty */                           { $$ = list_new(); }
    | VOID                                  { $$ = list_new(); }
    | param_list
    ;

param_list
    : param                                 { $$ = list_push(NULL, $1); }
    | param_list ',' param                  { $$ = list_push($1, $3); }
    ;

param
    : type_spec IDENT                       { $$ = mk_param($1, $2, @2.first_line); }
    | type_spec IDENT '[' ']'               { $$ = mk_param(array_of($1), $2, @2.first_line); }
    | type_spec IDENT '[' INT_LIT ']'       { $$ = mk_param(array_of($1), $2, @2.first_line); }
    ;

declaration
    : type_spec init_declarator_list ';'    { $$ = mk_decl($1, $2, @1.first_line); }
    ;

init_declarator_list
    : init_declarator                       { $$ = list_push(NULL, $1); }
    | init_declarator_list ',' init_declarator
                                            { $$ = list_push($1, $3); }
    ;

init_declarator
    : IDENT                                 { $$ = mk_var($1, 0, NULL, @1.first_line); }
    | IDENT '=' expr                        { $$ = mk_var($1, 0, $3, @1.first_line); }
    | IDENT '[' INT_LIT ']'                 { $$ = mk_var($1, (int)$3, NULL, @1.first_line); }
    ;

compound_stmt
    : '{' block_items '}'                   { $$ = mk_block($2, @1.first_line); }
    ;

block_items
    : /* empty */                           { $$ = list_new(); }
    | block_items block_item                { $$ = $2 ? list_push($1, $2) : $1; }
    ;

block_item
    : declaration
    | statement
    | error ';'                             { $$ = NULL; yyerrok; }
    ;

statement
    : compound_stmt
    | ';'                                   { $$ = node_new(N_EMPTY, @1.first_line); }
    | expr ';'                              { $$ = mk_wrap(N_EXPR_STMT, $1, @1.first_line); }
    | IF '(' expr ')' statement %prec THEN  { $$ = mk_if($3, $5, NULL, @1.first_line); }
    | IF '(' expr ')' statement ELSE statement
                                            { $$ = mk_if($3, $5, $7, @1.first_line); }
    | IF '(' error ')' statement %prec THEN { $$ = $5; yyerrok; }
    | WHILE '(' expr ')' statement          { $$ = mk_loop(N_WHILE, NULL, $3, NULL, $5, @1.first_line); }
    | WHILE '(' error ')' statement         { $$ = $5; yyerrok; }
    | DO statement WHILE '(' expr ')' ';'   { $$ = mk_loop(N_DO_WHILE, NULL, $5, NULL, $2, @1.first_line); }
    | FOR '(' for_init opt_expr ';' opt_expr ')' statement
                                            { $$ = mk_loop(N_FOR, $3, $4, $6, $8, @1.first_line); }
    | RETURN ';'                            { $$ = mk_wrap(N_RETURN, NULL, @1.first_line); }
    | RETURN expr ';'                       { $$ = mk_wrap(N_RETURN, $2, @1.first_line); }
    | BREAK ';'                             { $$ = node_new(N_BREAK, @1.first_line); }
    | CONTINUE ';'                          { $$ = node_new(N_CONTINUE, @1.first_line); }
    ;

for_init
    : ';'                                   { $$ = NULL; }
    | expr ';'                              { $$ = mk_wrap(N_EXPR_STMT, $1, @1.first_line); }
    | declaration
    ;

opt_expr
    : /* empty */                           { $$ = NULL; }
    | expr
    ;

expr
    : expr '=' expr                         { $$ = mk_assign(OP_NONE, $1, $3, @2.first_line); }
    | expr ADD_ASSIGN expr                  { $$ = mk_assign(OP_ADD, $1, $3, @2.first_line); }
    | expr SUB_ASSIGN expr                  { $$ = mk_assign(OP_SUB, $1, $3, @2.first_line); }
    | expr MUL_ASSIGN expr                  { $$ = mk_assign(OP_MUL, $1, $3, @2.first_line); }
    | expr DIV_ASSIGN expr                  { $$ = mk_assign(OP_DIV, $1, $3, @2.first_line); }
    | expr MOD_ASSIGN expr                  { $$ = mk_assign(OP_MOD, $1, $3, @2.first_line); }
    | expr '?' expr ':' expr                { $$ = mk_ternary($1, $3, $5, @2.first_line); }
    | expr OR expr                          { $$ = mk_logical(OP_OR, $1, $3, @2.first_line); }
    | expr AND expr                         { $$ = mk_logical(OP_AND, $1, $3, @2.first_line); }
    | expr EQ expr                          { $$ = mk_binary(OP_EQ, $1, $3, @2.first_line); }
    | expr NE expr                          { $$ = mk_binary(OP_NE, $1, $3, @2.first_line); }
    | expr '<' expr                         { $$ = mk_binary(OP_LT, $1, $3, @2.first_line); }
    | expr '>' expr                         { $$ = mk_binary(OP_GT, $1, $3, @2.first_line); }
    | expr LE expr                          { $$ = mk_binary(OP_LE, $1, $3, @2.first_line); }
    | expr GE expr                          { $$ = mk_binary(OP_GE, $1, $3, @2.first_line); }
    | expr '+' expr                         { $$ = mk_binary(OP_ADD, $1, $3, @2.first_line); }
    | expr '-' expr                         { $$ = mk_binary(OP_SUB, $1, $3, @2.first_line); }
    | expr '*' expr                         { $$ = mk_binary(OP_MUL, $1, $3, @2.first_line); }
    | expr '/' expr                         { $$ = mk_binary(OP_DIV, $1, $3, @2.first_line); }
    | expr '%' expr                         { $$ = mk_binary(OP_MOD, $1, $3, @2.first_line); }
    | '-' expr %prec UNARY                  { $$ = mk_unary(OP_NEG, $2, @1.first_line); }
    | '+' expr %prec UNARY                  { $$ = mk_unary(OP_POS, $2, @1.first_line); }
    | '!' expr %prec UNARY                  { $$ = mk_unary(OP_NOT, $2, @1.first_line); }
    | INC expr %prec UNARY                  { $$ = mk_incdec(OP_PRE_INC, $2, @1.first_line); }
    | DEC expr %prec UNARY                  { $$ = mk_incdec(OP_PRE_DEC, $2, @1.first_line); }
    | '(' type_spec ')' expr %prec UNARY    { $$ = mk_cast($2, $4, @1.first_line); }
    | expr INC                              { $$ = mk_incdec(OP_POST_INC, $1, @2.first_line); }
    | expr DEC                              { $$ = mk_incdec(OP_POST_DEC, $1, @2.first_line); }
    | expr '[' expr ']'                     { $$ = mk_index($1, $3, @2.first_line); }
    | primary
    ;

primary
    : INT_LIT                               { $$ = mk_int($1, @1.first_line); }
    | DOUBLE_LIT                            { $$ = mk_double(strtod($1, NULL), $1, @1.first_line); free($1); }
    | STRING_LIT                            { $$ = mk_string($1, @1.first_line); }
    | IDENT                                 { $$ = mk_ident($1, @1.first_line); }
    | IDENT '(' opt_args ')'                { $$ = mk_call($1, $3, @1.first_line); }
    | '(' expr ')'                          { $$ = $2; }
    ;

opt_args
    : /* empty */                           { $$ = list_new(); }
    | args
    ;

args
    : expr                                  { $$ = list_push(NULL, $1); }
    | args ',' expr                         { $$ = list_push($1, $3); }
    ;

%%

void yyerror(const char *msg)
{
    if (yytext && *yytext)
        diag_error(yylloc.first_line, "%s", msg);
    else
        diag_error(yylloc.first_line, "%s at end of input", msg);
}
