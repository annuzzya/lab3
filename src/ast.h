#ifndef AST_H
#define AST_H

#include <stdio.h>

typedef enum {
    T_VOID,
    T_INT,
    T_DOUBLE,
    T_INT_ARRAY,
    T_DOUBLE_ARRAY,
    T_STRING,
    T_ERROR
} Type;

typedef enum {
    N_PROGRAM,
    N_FUNC,
    N_PARAM,
    N_DECL,
    N_VAR,
    N_BLOCK,
    N_IF,
    N_WHILE,
    N_DO_WHILE,
    N_FOR,
    N_RETURN,
    N_BREAK,
    N_CONTINUE,
    N_EXPR_STMT,
    N_EMPTY,
    N_INT_LIT,
    N_DOUBLE_LIT,
    N_STRING_LIT,
    N_IDENT,
    N_INDEX,
    N_CALL,
    N_UNARY,
    N_BINARY,
    N_LOGICAL,
    N_ASSIGN,
    N_INCDEC,
    N_TERNARY,
    N_CAST
} NodeKind;

typedef enum {
    OP_NONE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_EQ,
    OP_NE,
    OP_AND,
    OP_OR,
    OP_NEG,
    OP_POS,
    OP_NOT,
    OP_PRE_INC,
    OP_PRE_DEC,
    OP_POST_INC,
    OP_POST_DEC
} Op;

typedef enum {
    BI_NONE,
    BI_PRINTF,
    BI_SQRT,
    BI_FABS,
    BI_POW
} Builtin;

typedef struct Node Node;
typedef struct Symbol Symbol;

typedef struct NodeList {
    Node **items;
    int len;
    int cap;
} NodeList;

struct Symbol {
    char *name;
    Type type;
    int global;
    int slot;
    int size;
    Node *decl;
};

struct Node {
    NodeKind kind;
    int line;
    Type type;
    union {
        struct { NodeList *items; } list;
        struct { Type ret; char *name; NodeList *params; Node *body; Node *def; int nlocals; } func;
        struct { Type type; char *name; int size; Node *init; Symbol *sym; } var;
        struct { Type type; NodeList *vars; } decl;
        struct { Node *cond; Node *then; Node *els; } branch;
        struct { Node *init; Node *cond; Node *step; Node *body; } loop;
        struct { Node *expr; } wrap;
        struct { long value; } ilit;
        struct { double value; char *text; } dlit;
        struct { char *value; } slit;
        struct { char *name; Symbol *sym; } ident;
        struct { Node *base; Node *index; } index;
        struct { char *name; NodeList *args; Node *callee; int builtin; } call;
        struct { Op op; Node *lhs; Node *rhs; } binary;
        struct { Op op; Node *operand; } unary;
    } u;
};

NodeList *list_new(void);
NodeList *list_push(NodeList *list, Node *node);
void list_free(NodeList *list);

Node *node_new(NodeKind kind, int line);
Node *mk_int(long value, int line);
Node *mk_double(double value, const char *text, int line);
Node *mk_string(char *value, int line);
Node *mk_ident(char *name, int line);
Node *mk_index(Node *base, Node *index, int line);
Node *mk_call(char *name, NodeList *args, int line);
Node *mk_unary(Op op, Node *operand, int line);
Node *mk_incdec(Op op, Node *operand, int line);
Node *mk_cast(Type type, Node *operand, int line);
Node *mk_binary(Op op, Node *lhs, Node *rhs, int line);
Node *mk_logical(Op op, Node *lhs, Node *rhs, int line);
Node *mk_assign(Op op, Node *lhs, Node *rhs, int line);
Node *mk_ternary(Node *cond, Node *then, Node *els, int line);
Node *mk_var(char *name, int size, Node *init, int line);
Node *mk_decl(Type type, NodeList *vars, int line);
Node *mk_param(Type type, char *name, int line);
Node *mk_func(Type ret, char *name, NodeList *params, Node *body, int line);
Node *mk_block(NodeList *items, int line);
Node *mk_program(NodeList *items);
Node *mk_if(Node *cond, Node *then, Node *els, int line);
Node *mk_loop(NodeKind kind, Node *init, Node *cond, Node *step, Node *body, int line);
Node *mk_wrap(NodeKind kind, Node *expr, int line);

void ast_free(Node *node);
void ast_print(FILE *out, const Node *root);
void ast_json(FILE *out, const Node *root);
void json_string(FILE *out, const char *s);

const char *type_name(Type type);
const char *op_symbol(Op op);
const char *kind_name(NodeKind kind);
Type elem_type(Type type);
Type array_of(Type type);
int is_array(Type type);
int is_numeric(Type type);

#endif
