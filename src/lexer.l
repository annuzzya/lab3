%{
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "diag.h"
#include "parser.tab.h"

#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;

static char *unescape(const char *src, int len);
static long char_value(const char *text);
%}

%option noyywrap nounput noinput yylineno

%x COMMENT

DIGIT   [0-9]
LETTER  [A-Za-z_]
EXP     [eE][+-]?{DIGIT}+

%%

^[ \t]*#[^\n]*              ;
"//"[^\n]*                  ;
"/*"                        BEGIN(COMMENT);
<COMMENT>"*/"               BEGIN(INITIAL);
<COMMENT>\n                 ;
<COMMENT>.                  ;
<COMMENT><<EOF>>            { diag_error(yylineno, "unterminated comment"); return 0; }

"int"                       return INT;
"double"                    return DOUBLE;
"void"                      return VOID;
"if"                        return IF;
"else"                      return ELSE;
"while"                     return WHILE;
"do"                        return DO;
"for"                       return FOR;
"return"                    return RETURN;
"break"                     return BREAK;
"continue"                  return CONTINUE;

{LETTER}({LETTER}|{DIGIT})* { yylval.str = strdup(yytext); return IDENT; }

0[xX][0-9a-fA-F]+           { yylval.ival = strtol(yytext, NULL, 16); return INT_LIT; }
{DIGIT}+                    { yylval.ival = strtol(yytext, NULL, 10); return INT_LIT; }
{DIGIT}+"."{DIGIT}*{EXP}?   |
"."{DIGIT}+{EXP}?           |
{DIGIT}+{EXP}               { yylval.str = strdup(yytext); return DOUBLE_LIT; }

'(\\.|[^\\'\n])'            { yylval.ival = char_value(yytext + 1); return INT_LIT; }
\"(\\.|[^\\"\n])*\"         { yylval.str = unescape(yytext + 1, yyleng - 2); return STRING_LIT; }
\"(\\.|[^\\"\n])*           { diag_error(yylineno, "unterminated string literal"); }

"++"                        return INC;
"--"                        return DEC;
"<="                        return LE;
">="                        return GE;
"=="                        return EQ;
"!="                        return NE;
"&&"                        return AND;
"||"                        return OR;
"+="                        return ADD_ASSIGN;
"-="                        return SUB_ASSIGN;
"*="                        return MUL_ASSIGN;
"/="                        return DIV_ASSIGN;
"%="                        return MOD_ASSIGN;

[-+*/%<>=!?:;,(){}\[\]]     return yytext[0];

[ \t\r\n]+                  ;
.                           diag_error(yylineno, "unexpected character '%s'", yytext);

%%

static int escape_char(char c)
{
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '0': return '\0';
    case 'a': return '\a';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'v': return '\v';
    default: return c;
    }
}

static long char_value(const char *text)
{
    return text[0] == '\\' ? escape_char(text[1]) : (unsigned char)text[0];
}

static char *unescape(const char *src, int len)
{
    char *out = malloc(len > 0 ? (size_t)len + 1 : 1);
    int j = 0;
    for (int i = 0; i < len; i++) {
        if (src[i] == '\\' && i + 1 < len)
            out[j++] = (char)escape_char(src[++i]);
        else
            out[j++] = src[i];
    }
    out[j] = '\0';
    return out;
}

const char *token_name(int token)
{
    switch (token) {
    case INT: return "INT";
    case DOUBLE: return "DOUBLE";
    case VOID: return "VOID";
    case IF: return "IF";
    case ELSE: return "ELSE";
    case WHILE: return "WHILE";
    case DO: return "DO";
    case FOR: return "FOR";
    case RETURN: return "RETURN";
    case BREAK: return "BREAK";
    case CONTINUE: return "CONTINUE";
    case IDENT: return "IDENT";
    case INT_LIT: return "INT_LIT";
    case DOUBLE_LIT: return "DOUBLE_LIT";
    case STRING_LIT: return "STRING_LIT";
    case INC: return "INC";
    case DEC: return "DEC";
    case LE: return "LE";
    case GE: return "GE";
    case EQ: return "EQ";
    case NE: return "NE";
    case AND: return "AND";
    case OR: return "OR";
    case ADD_ASSIGN: return "ADD_ASSIGN";
    case SUB_ASSIGN: return "SUB_ASSIGN";
    case MUL_ASSIGN: return "MUL_ASSIGN";
    case DIV_ASSIGN: return "DIV_ASSIGN";
    case MOD_ASSIGN: return "MOD_ASSIGN";
    default: return "PUNCT";
    }
}

void lexer_reset(FILE *in)
{
    yyin = in;
    yylineno = 1;
    BEGIN(INITIAL);
    yyrestart(in);
}

void dump_tokens(FILE *in, FILE *out)
{
    int token;
    lexer_reset(in);
    while ((token = yylex()) != 0) {
        fprintf(out, "%4d  %-12s %s\n", yylineno, token_name(token), yytext);
        if (token == IDENT || token == DOUBLE_LIT || token == STRING_LIT)
            free(yylval.str);
    }
}
