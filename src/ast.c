#include "ast.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *role;
    Node *node;
} Child;

#define MAX_FIXED_CHILDREN 4

static void *xmalloc(size_t size)
{
    void *p = calloc(1, size);
    if (!p) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
    return p;
}

NodeList *list_new(void)
{
    return xmalloc(sizeof(NodeList));
}

NodeList *list_push(NodeList *list, Node *node)
{
    if (!list)
        list = list_new();
    if (list->len == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 4;
        list->items = realloc(list->items, sizeof(Node *) * list->cap);
        if (!list->items) {
            fprintf(stderr, "out of memory\n");
            exit(2);
        }
    }
    list->items[list->len++] = node;
    return list;
}

Node *node_new(NodeKind kind, int line)
{
    Node *n = xmalloc(sizeof(Node));
    n->kind = kind;
    n->line = line;
    n->type = T_VOID;
    return n;
}

Node *mk_int(long value, int line)
{
    Node *n = node_new(N_INT_LIT, line);
    n->u.ilit.value = value;
    return n;
}

Node *mk_double(double value, const char *text, int line)
{
    Node *n = node_new(N_DOUBLE_LIT, line);
    n->u.dlit.value = value;
    n->u.dlit.text = strdup(text);
    return n;
}

Node *mk_string(char *value, int line)
{
    Node *n = node_new(N_STRING_LIT, line);
    n->u.slit.value = value;
    return n;
}

Node *mk_ident(char *name, int line)
{
    Node *n = node_new(N_IDENT, line);
    n->u.ident.name = name;
    return n;
}

Node *mk_index(Node *base, Node *index, int line)
{
    Node *n = node_new(N_INDEX, line);
    n->u.index.base = base;
    n->u.index.index = index;
    return n;
}

Node *mk_call(char *name, NodeList *args, int line)
{
    Node *n = node_new(N_CALL, line);
    n->u.call.name = name;
    n->u.call.args = args ? args : list_new();
    return n;
}

Node *mk_unary(Op op, Node *operand, int line)
{
    Node *n = node_new(N_UNARY, line);
    n->u.unary.op = op;
    n->u.unary.operand = operand;
    return n;
}

Node *mk_incdec(Op op, Node *operand, int line)
{
    Node *n = mk_unary(op, operand, line);
    n->kind = N_INCDEC;
    return n;
}

Node *mk_cast(Type type, Node *operand, int line)
{
    Node *n = mk_unary(OP_NONE, operand, line);
    n->kind = N_CAST;
    n->type = type;
    return n;
}

Node *mk_binary(Op op, Node *lhs, Node *rhs, int line)
{
    Node *n = node_new(N_BINARY, line);
    n->u.binary.op = op;
    n->u.binary.lhs = lhs;
    n->u.binary.rhs = rhs;
    return n;
}

Node *mk_logical(Op op, Node *lhs, Node *rhs, int line)
{
    Node *n = mk_binary(op, lhs, rhs, line);
    n->kind = N_LOGICAL;
    return n;
}

Node *mk_assign(Op op, Node *lhs, Node *rhs, int line)
{
    Node *n = mk_binary(op, lhs, rhs, line);
    n->kind = N_ASSIGN;
    return n;
}

Node *mk_ternary(Node *cond, Node *then, Node *els, int line)
{
    Node *n = mk_if(cond, then, els, line);
    n->kind = N_TERNARY;
    return n;
}

Node *mk_var(char *name, int size, Node *init, int line)
{
    Node *n = node_new(N_VAR, line);
    n->u.var.name = name;
    n->u.var.size = size;
    n->u.var.init = init;
    return n;
}

Node *mk_decl(Type type, NodeList *vars, int line)
{
    Node *n = node_new(N_DECL, line);
    n->u.decl.type = type;
    n->u.decl.vars = vars;
    for (int i = 0; i < vars->len; i++) {
        Node *v = vars->items[i];
        v->u.var.type = v->u.var.size > 0 ? array_of(type) : type;
    }
    return n;
}

Node *mk_param(Type type, char *name, int line)
{
    Node *n = node_new(N_PARAM, line);
    n->u.var.type = type;
    n->u.var.name = name;
    return n;
}

Node *mk_func(Type ret, char *name, NodeList *params, Node *body, int line)
{
    Node *n = node_new(N_FUNC, line);
    n->u.func.ret = ret;
    n->u.func.name = name;
    n->u.func.params = params ? params : list_new();
    n->u.func.body = body;
    return n;
}

Node *mk_block(NodeList *items, int line)
{
    Node *n = node_new(N_BLOCK, line);
    n->u.list.items = items ? items : list_new();
    return n;
}

Node *mk_program(NodeList *items)
{
    Node *n = mk_block(items, 1);
    n->kind = N_PROGRAM;
    return n;
}

Node *mk_if(Node *cond, Node *then, Node *els, int line)
{
    Node *n = node_new(N_IF, line);
    n->u.branch.cond = cond;
    n->u.branch.then = then;
    n->u.branch.els = els;
    return n;
}

Node *mk_loop(NodeKind kind, Node *init, Node *cond, Node *step, Node *body, int line)
{
    Node *n = node_new(kind, line);
    n->u.loop.init = init;
    n->u.loop.cond = cond;
    n->u.loop.step = step;
    n->u.loop.body = body;
    return n;
}

Node *mk_wrap(NodeKind kind, Node *expr, int line)
{
    Node *n = node_new(kind, line);
    n->u.wrap.expr = expr;
    return n;
}

const char *type_name(Type type)
{
    switch (type) {
    case T_VOID: return "void";
    case T_INT: return "int";
    case T_DOUBLE: return "double";
    case T_INT_ARRAY: return "int[]";
    case T_DOUBLE_ARRAY: return "double[]";
    case T_STRING: return "char *";
    case T_ERROR: return "<error>";
    }
    return "?";
}

Type elem_type(Type type)
{
    return type == T_INT_ARRAY ? T_INT : type == T_DOUBLE_ARRAY ? T_DOUBLE : T_ERROR;
}

Type array_of(Type type)
{
    return type == T_INT ? T_INT_ARRAY : type == T_DOUBLE ? T_DOUBLE_ARRAY : T_ERROR;
}

int is_array(Type type)
{
    return type == T_INT_ARRAY || type == T_DOUBLE_ARRAY;
}

int is_numeric(Type type)
{
    return type == T_INT || type == T_DOUBLE;
}

const char *op_symbol(Op op)
{
    switch (op) {
    case OP_NONE: return "=";
    case OP_ADD: return "+";
    case OP_SUB: return "-";
    case OP_MUL: return "*";
    case OP_DIV: return "/";
    case OP_MOD: return "%";
    case OP_LT: return "<";
    case OP_GT: return ">";
    case OP_LE: return "<=";
    case OP_GE: return ">=";
    case OP_EQ: return "==";
    case OP_NE: return "!=";
    case OP_AND: return "&&";
    case OP_OR: return "||";
    case OP_NEG: return "-";
    case OP_POS: return "+";
    case OP_NOT: return "!";
    case OP_PRE_INC:
    case OP_POST_INC: return "++";
    case OP_PRE_DEC:
    case OP_POST_DEC: return "--";
    }
    return "?";
}

const char *kind_name(NodeKind kind)
{
    switch (kind) {
    case N_PROGRAM: return "TranslationUnit";
    case N_FUNC: return "FunctionDecl";
    case N_PARAM: return "ParmVarDecl";
    case N_DECL: return "DeclStmt";
    case N_VAR: return "VarDecl";
    case N_BLOCK: return "CompoundStmt";
    case N_IF: return "IfStmt";
    case N_WHILE: return "WhileStmt";
    case N_DO_WHILE: return "DoStmt";
    case N_FOR: return "ForStmt";
    case N_RETURN: return "ReturnStmt";
    case N_BREAK: return "BreakStmt";
    case N_CONTINUE: return "ContinueStmt";
    case N_EXPR_STMT: return "ExprStmt";
    case N_EMPTY: return "NullStmt";
    case N_INT_LIT: return "IntegerLiteral";
    case N_DOUBLE_LIT: return "FloatingLiteral";
    case N_STRING_LIT: return "StringLiteral";
    case N_IDENT: return "DeclRefExpr";
    case N_INDEX: return "ArraySubscriptExpr";
    case N_CALL: return "CallExpr";
    case N_UNARY: return "UnaryOperator";
    case N_BINARY: return "BinaryOperator";
    case N_LOGICAL: return "LogicalOperator";
    case N_ASSIGN: return "AssignOperator";
    case N_INCDEC: return "IncDecOperator";
    case N_TERNARY: return "ConditionalOperator";
    case N_CAST: return "CStyleCastExpr";
    }
    return "?";
}

static int is_expr(NodeKind kind)
{
    return kind >= N_INT_LIT;
}

static void escape_into(char *dst, size_t size, const char *src)
{
    size_t j = 0;
    for (const char *p = src; *p && j + 3 < size; p++) {
        char e = 0;
        switch (*p) {
        case '\n': e = 'n'; break;
        case '\t': e = 't'; break;
        case '"': e = '"'; break;
        case '\\': e = '\\'; break;
        }
        if (e) {
            dst[j++] = '\\';
            dst[j++] = e;
        } else {
            dst[j++] = *p;
        }
    }
    dst[j] = '\0';
}

static void node_label(const Node *n, char *buf, size_t size)
{
    char tmp[160];
    switch (n->kind) {
    case N_FUNC: {
        int off = snprintf(buf, size, "%s %s(", type_name(n->u.func.ret), n->u.func.name);
        NodeList *ps = n->u.func.params;
        for (int i = 0; i < ps->len && off < (int)size; i++)
            off += snprintf(buf + off, size - off, "%s%s", i ? ", " : "", type_name(ps->items[i]->u.var.type));
        if (off < (int)size)
            snprintf(buf + off, size - off, ")%s", n->u.func.body ? "" : " prototype");
        return;
    }
    case N_PARAM:
    case N_VAR:
        if (n->u.var.size > 0)
            snprintf(buf, size, "%s %s[%d]", type_name(elem_type(n->u.var.type)), n->u.var.name, n->u.var.size);
        else
            snprintf(buf, size, "%s %s", type_name(n->u.var.type), n->u.var.name);
        return;
    case N_DECL:
        snprintf(buf, size, "%s", type_name(n->u.decl.type));
        return;
    case N_INT_LIT:
        snprintf(buf, size, "%ld", n->u.ilit.value);
        return;
    case N_DOUBLE_LIT:
        snprintf(buf, size, "%s", n->u.dlit.text);
        return;
    case N_STRING_LIT:
        escape_into(tmp, sizeof tmp, n->u.slit.value);
        snprintf(buf, size, "\"%s\"", tmp);
        return;
    case N_IDENT:
        snprintf(buf, size, "%s", n->u.ident.name);
        return;
    case N_CALL:
        snprintf(buf, size, "%s", n->u.call.name);
        return;
    case N_UNARY:
    case N_BINARY:
    case N_LOGICAL:
        snprintf(buf, size, "%s", op_symbol(n->u.binary.op));
        return;
    case N_ASSIGN:
        snprintf(buf, size, "%s=", n->u.binary.op == OP_NONE ? "" : op_symbol(n->u.binary.op));
        return;
    case N_INCDEC:
        snprintf(buf, size, "%s %s", op_symbol(n->u.unary.op),
                 n->u.unary.op == OP_POST_INC || n->u.unary.op == OP_POST_DEC ? "postfix" : "prefix");
        return;
    case N_CAST:
        snprintf(buf, size, "(%s)", type_name(n->type));
        return;
    default:
        buf[0] = '\0';
    }
}

static int node_children(const Node *n, Child *fixed, NodeList **list, const char **list_role)
{
    int k = 0;
    *list = NULL;
    *list_role = "";
#define ADD(r, c) do { if (c) { fixed[k].role = (r); fixed[k].node = (c); k++; } } while (0)
    switch (n->kind) {
    case N_PROGRAM:
    case N_BLOCK:
        *list = n->u.list.items;
        break;
    case N_FUNC:
        *list = n->u.func.params;
        *list_role = "param";
        ADD("body", n->u.func.body);
        break;
    case N_DECL:
        *list = n->u.decl.vars;
        break;
    case N_VAR:
        ADD("init", n->u.var.init);
        break;
    case N_IF:
    case N_TERNARY:
        ADD("cond", n->u.branch.cond);
        ADD("then", n->u.branch.then);
        ADD("else", n->u.branch.els);
        break;
    case N_WHILE:
    case N_DO_WHILE:
    case N_FOR:
        ADD("init", n->u.loop.init);
        ADD("cond", n->u.loop.cond);
        ADD("step", n->u.loop.step);
        ADD("body", n->u.loop.body);
        break;
    case N_RETURN:
    case N_EXPR_STMT:
        ADD("", n->u.wrap.expr);
        break;
    case N_INDEX:
        ADD("base", n->u.index.base);
        ADD("index", n->u.index.index);
        break;
    case N_CALL:
        *list = n->u.call.args;
        *list_role = "arg";
        break;
    case N_UNARY:
    case N_INCDEC:
    case N_CAST:
        ADD("", n->u.unary.operand);
        break;
    case N_BINARY:
    case N_LOGICAL:
    case N_ASSIGN:
        ADD("lhs", n->u.binary.lhs);
        ADD("rhs", n->u.binary.rhs);
        break;
    default:
        break;
    }
#undef ADD
    return k;
}

static void print_rec(FILE *out, const Node *n, const char *prefix, int last, const char *role, int root)
{
    char label[256];
    node_label(n, label, sizeof label);
    fprintf(out, "%s%s", prefix, root ? "" : last ? "`-- " : "|-- ");
    if (role && *role)
        fprintf(out, "%s: ", role);
    fprintf(out, "%s", kind_name(n->kind));
    if (*label)
        fprintf(out, " %s", label);
    if (is_expr(n->kind) && n->type != T_VOID)
        fprintf(out, " <%s>", type_name(n->type));
    fprintf(out, "  [line %d]\n", n->line);

    char next[1024];
    snprintf(next, sizeof next, "%s%s", prefix, root ? "" : last ? "    " : "|   ");

    Child fixed[MAX_FIXED_CHILDREN];
    NodeList *list;
    const char *list_role;
    int k = node_children(n, fixed, &list, &list_role);
    int total = k + (list ? list->len : 0);
    int idx = 0;
    if (list) {
        for (int i = 0; i < list->len; i++, idx++)
            print_rec(out, list->items[i], next, idx == total - 1, list_role, 0);
    }
    for (int i = 0; i < k; i++, idx++)
        print_rec(out, fixed[i].node, next, idx == total - 1, fixed[i].role, 0);
}

void ast_print(FILE *out, const Node *root)
{
    print_rec(out, root, "", 1, NULL, 1);
}

void json_string(FILE *out, const char *s)
{
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out); break;
        case '\t': fputs("\\t", out); break;
        case '\r': fputs("\\r", out); break;
        default:
            if (*p < 0x20)
                fprintf(out, "\\u%04x", *p);
            else
                fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void json_rec(FILE *out, const Node *n, const char *role)
{
    char label[256];
    node_label(n, label, sizeof label);
    fputs("{\"kind\":", out);
    json_string(out, kind_name(n->kind));
    fputs(",\"label\":", out);
    json_string(out, label);
    fprintf(out, ",\"line\":%d", n->line);
    if (is_expr(n->kind) && n->type != T_VOID) {
        fputs(",\"type\":", out);
        json_string(out, type_name(n->type));
    }
    if (role && *role) {
        fputs(",\"role\":", out);
        json_string(out, role);
    }

    Child fixed[MAX_FIXED_CHILDREN];
    NodeList *list;
    const char *list_role;
    int k = node_children(n, fixed, &list, &list_role);
    if (k || (list && list->len)) {
        fputs(",\"children\":[", out);
        int first = 1;
        if (list) {
            for (int i = 0; i < list->len; i++, first = 0) {
                if (!first)
                    fputc(',', out);
                json_rec(out, list->items[i], list_role);
            }
        }
        for (int i = 0; i < k; i++, first = 0) {
            if (!first)
                fputc(',', out);
            json_rec(out, fixed[i].node, fixed[i].role);
        }
        fputc(']', out);
    }
    fputc('}', out);
}

void ast_json(FILE *out, const Node *root)
{
    json_rec(out, root, NULL);
}

void list_free(NodeList *list)
{
    if (!list)
        return;
    for (int i = 0; i < list->len; i++)
        ast_free(list->items[i]);
    free(list->items);
    free(list);
}

void ast_free(Node *n)
{
    if (!n)
        return;
    switch (n->kind) {
    case N_PROGRAM:
    case N_BLOCK:
        list_free(n->u.list.items);
        break;
    case N_FUNC:
        free(n->u.func.name);
        list_free(n->u.func.params);
        ast_free(n->u.func.body);
        break;
    case N_PARAM:
    case N_VAR:
        free(n->u.var.name);
        ast_free(n->u.var.init);
        if (n->u.var.sym) {
            free(n->u.var.sym->name);
            free(n->u.var.sym);
        }
        break;
    case N_DECL:
        list_free(n->u.decl.vars);
        break;
    case N_IF:
    case N_TERNARY:
        ast_free(n->u.branch.cond);
        ast_free(n->u.branch.then);
        ast_free(n->u.branch.els);
        break;
    case N_WHILE:
    case N_DO_WHILE:
    case N_FOR:
        ast_free(n->u.loop.init);
        ast_free(n->u.loop.cond);
        ast_free(n->u.loop.step);
        ast_free(n->u.loop.body);
        break;
    case N_RETURN:
    case N_EXPR_STMT:
        ast_free(n->u.wrap.expr);
        break;
    case N_DOUBLE_LIT:
        free(n->u.dlit.text);
        break;
    case N_STRING_LIT:
        free(n->u.slit.value);
        break;
    case N_IDENT:
        free(n->u.ident.name);
        break;
    case N_INDEX:
        ast_free(n->u.index.base);
        ast_free(n->u.index.index);
        break;
    case N_CALL:
        free(n->u.call.name);
        list_free(n->u.call.args);
        break;
    case N_UNARY:
    case N_INCDEC:
    case N_CAST:
        ast_free(n->u.unary.operand);
        break;
    case N_BINARY:
    case N_LOGICAL:
    case N_ASSIGN:
        ast_free(n->u.binary.lhs);
        ast_free(n->u.binary.rhs);
        break;
    default:
        break;
    }
    free(n);
}
