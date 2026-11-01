#include "sema.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "diag.h"
#include "format.h"

typedef struct Scope {
    Symbol **syms;
    int len;
    int cap;
    struct Scope *parent;
} Scope;

typedef struct {
    char *name;
    Node *decl;
    Node *def;
    int used_line;
} FuncEntry;

static Scope *scope;
static FuncEntry *funcs;
static int nfuncs;
static Node *current_fn;
static int next_slot;
static int max_slot;
static int nglobals;
static int loop_depth;

static void check_stmt(Node *n);
static Type check_expr(Node *n);

static void scope_push(void)
{
    Scope *s = calloc(1, sizeof(Scope));
    s->parent = scope;
    scope = s;
}

static void scope_pop(void)
{
    Scope *s = scope;
    scope = s->parent;
    free(s->syms);
    free(s);
}

static Symbol *scope_find(Scope *s, const char *name)
{
    for (int i = 0; i < s->len; i++)
        if (strcmp(s->syms[i]->name, name) == 0)
            return s->syms[i];
    return NULL;
}

static Symbol *lookup(const char *name)
{
    for (Scope *s = scope; s; s = s->parent) {
        Symbol *sym = scope_find(s, name);
        if (sym)
            return sym;
    }
    return NULL;
}

static FuncEntry *find_func(const char *name)
{
    for (int i = 0; i < nfuncs; i++)
        if (strcmp(funcs[i].name, name) == 0)
            return &funcs[i];
    return NULL;
}

static Builtin find_builtin(const char *name)
{
    if (strcmp(name, "printf") == 0) return BI_PRINTF;
    if (strcmp(name, "sqrt") == 0) return BI_SQRT;
    if (strcmp(name, "fabs") == 0) return BI_FABS;
    if (strcmp(name, "pow") == 0) return BI_POW;
    return BI_NONE;
}

static void declare(Node *decl)
{
    const char *name = decl->u.var.name;
    if (scope_find(scope, name)) {
        diag_error(decl->line, "redefinition of '%s'", name);
        return;
    }
    if (!scope->parent && find_func(name)) {
        diag_error(decl->line, "'%s' redeclared as a different kind of symbol", name);
        return;
    }
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = strdup(name);
    sym->type = decl->u.var.type;
    sym->size = decl->u.var.size;
    sym->decl = decl;
    sym->global = scope->parent == NULL;
    if (sym->global) {
        sym->slot = nglobals++;
    } else {
        sym->slot = next_slot++;
        if (next_slot > max_slot)
            max_slot = next_slot;
    }
    decl->u.var.sym = sym;
    if (scope->len == scope->cap) {
        scope->cap = scope->cap ? scope->cap * 2 : 8;
        scope->syms = realloc(scope->syms, sizeof(Symbol *) * scope->cap);
    }
    scope->syms[scope->len++] = sym;
}

static int is_lvalue(const Node *n)
{
    if (n->kind == N_INDEX)
        return 1;
    return n->kind == N_IDENT && n->u.ident.sym && !is_array(n->u.ident.sym->type);
}

static int is_constant(const Node *n)
{
    switch (n->kind) {
    case N_INT_LIT:
    case N_DOUBLE_LIT:
        return 1;
    case N_UNARY:
    case N_CAST:
        return is_constant(n->u.unary.operand);
    case N_BINARY:
    case N_LOGICAL:
        return is_constant(n->u.binary.lhs) && is_constant(n->u.binary.rhs);
    case N_TERNARY:
        return is_constant(n->u.branch.cond) && is_constant(n->u.branch.then) && is_constant(n->u.branch.els);
    default:
        return 0;
    }
}

static Type arith(Type a, Type b)
{
    return a == T_DOUBLE || b == T_DOUBLE ? T_DOUBLE : T_INT;
}

static int require_scalar(const Node *n, Type t, const char *what)
{
    if (t == T_ERROR)
        return 0;
    if (!is_numeric(t)) {
        diag_error(n->line, "%s must have arithmetic type, got '%s'", what, type_name(t));
        return 0;
    }
    return 1;
}

static void check_printf(Node *n)
{
    NodeList *args = n->u.call.args;
    if (args->len == 0 || args->items[0]->kind != N_STRING_LIT) {
        diag_error(n->line, "first argument of printf must be a string literal");
        for (int i = 0; i < args->len; i++)
            check_expr(args->items[i]);
        return;
    }
    args->items[0]->type = T_STRING;
    const char *fmt = args->items[0]->u.slit.value;
    FormatSpec spec;
    int arg = 1;
    while ((fmt = format_next(fmt, &spec)) != NULL) {
        Type want = format_arg_type(spec.conv);
        if (want == T_VOID)
            continue;
        if (want == T_ERROR) {
            diag_error(n->line, "unsupported conversion '%.*s' in printf format", spec.len, spec.start);
            continue;
        }
        if (arg >= args->len) {
            diag_error(n->line, "format '%.*s' expects a matching argument", spec.len, spec.start);
            continue;
        }
        Node *a = args->items[arg++];
        Type got = check_expr(a);
        if (got != T_ERROR && got != want)
            diag_error(a->line, "format '%.*s' expects '%s', but argument has type '%s'",
                       spec.len, spec.start, type_name(want), type_name(got));
    }
    for (; arg < args->len; arg++) {
        check_expr(args->items[arg]);
        diag_error(args->items[arg]->line, "data argument not used by format string");
    }
}

static Type check_call(Node *n)
{
    NodeList *args = n->u.call.args;
    Builtin bi = find_builtin(n->u.call.name);
    FuncEntry *fe = find_func(n->u.call.name);

    if (!fe && bi != BI_NONE) {
        n->u.call.builtin = bi;
        if (bi == BI_PRINTF) {
            check_printf(n);
            return T_INT;
        }
        int want = bi == BI_POW ? 2 : 1;
        if (args->len != want)
            diag_error(n->line, "'%s' expects %d argument%s, got %d", n->u.call.name, want, want == 1 ? "" : "s", args->len);
        for (int i = 0; i < args->len; i++)
            require_scalar(args->items[i], check_expr(args->items[i]), "argument");
        return T_DOUBLE;
    }
    if (!fe) {
        diag_error(n->line, "call to undeclared function '%s'", n->u.call.name);
        for (int i = 0; i < args->len; i++)
            check_expr(args->items[i]);
        return T_ERROR;
    }
    if (!fe->used_line)
        fe->used_line = n->line;
    n->u.call.callee = fe->decl;
    NodeList *params = fe->decl->u.func.params;
    if (args->len != params->len) {
        diag_error(n->line, "function '%s' expects %d argument%s, got %d", n->u.call.name,
                   params->len, params->len == 1 ? "" : "s", args->len);
    }
    for (int i = 0; i < args->len; i++) {
        Type got = check_expr(args->items[i]);
        if (i >= params->len || got == T_ERROR)
            continue;
        Type want = params->items[i]->u.var.type;
        if (is_array(want) || is_array(got)) {
            if (want != got)
                diag_error(args->items[i]->line, "argument %d of '%s': cannot pass '%s' as '%s'",
                           i + 1, n->u.call.name, type_name(got), type_name(want));
        } else {
            require_scalar(args->items[i], got, "argument");
        }
    }
    return fe->decl->u.func.ret;
}

static Type check_expr_inner(Node *n)
{
    switch (n->kind) {
    case N_INT_LIT:
        if (n->u.ilit.value > INT_MAX)
            diag_error(n->line, "integer literal is too large for type 'int'");
        return T_INT;
    case N_DOUBLE_LIT:
        return T_DOUBLE;
    case N_STRING_LIT:
        return T_STRING;
    case N_IDENT: {
        Symbol *sym = lookup(n->u.ident.name);
        if (!sym) {
            if (find_func(n->u.ident.name))
                diag_error(n->line, "function '%s' used as a value", n->u.ident.name);
            else
                diag_error(n->line, "use of undeclared identifier '%s'", n->u.ident.name);
            return T_ERROR;
        }
        n->u.ident.sym = sym;
        return sym->type;
    }
    case N_INDEX: {
        Type base = check_expr(n->u.index.base);
        Type idx = check_expr(n->u.index.index);
        if (idx != T_ERROR && idx != T_INT)
            diag_error(n->u.index.index->line, "array subscript is not an integer");
        if (base == T_ERROR)
            return T_ERROR;
        if (!is_array(base)) {
            diag_error(n->line, "subscripted value is not an array");
            return T_ERROR;
        }
        return elem_type(base);
    }
    case N_CALL:
        return check_call(n);
    case N_UNARY: {
        Type t = check_expr(n->u.unary.operand);
        if (!require_scalar(n->u.unary.operand, t, "operand"))
            return T_ERROR;
        return n->u.unary.op == OP_NOT ? T_INT : t;
    }
    case N_CAST: {
        Type t = check_expr(n->u.unary.operand);
        if (n->type != T_INT && n->type != T_DOUBLE) {
            diag_error(n->line, "cast to '%s' is not supported", type_name(n->type));
            return T_ERROR;
        }
        if (!require_scalar(n->u.unary.operand, t, "operand of cast"))
            return T_ERROR;
        return n->type;
    }
    case N_INCDEC: {
        Type t = check_expr(n->u.unary.operand);
        if (t == T_ERROR)
            return T_ERROR;
        if (!is_lvalue(n->u.unary.operand)) {
            diag_error(n->line, "expression is not assignable");
            return T_ERROR;
        }
        return require_scalar(n->u.unary.operand, t, "operand of ++/--") ? t : T_ERROR;
    }
    case N_BINARY: {
        Type a = check_expr(n->u.binary.lhs);
        Type b = check_expr(n->u.binary.rhs);
        int ok = require_scalar(n->u.binary.lhs, a, "left operand");
        ok = require_scalar(n->u.binary.rhs, b, "right operand") && ok;
        if (!ok)
            return T_ERROR;
        switch (n->u.binary.op) {
        case OP_MOD:
            if (a != T_INT || b != T_INT) {
                diag_error(n->line, "invalid operands to '%%' ('%s' and '%s')", type_name(a), type_name(b));
                return T_ERROR;
            }
            return T_INT;
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
            return arith(a, b);
        default:
            return T_INT;
        }
    }
    case N_LOGICAL: {
        Type a = check_expr(n->u.binary.lhs);
        Type b = check_expr(n->u.binary.rhs);
        int ok = require_scalar(n->u.binary.lhs, a, "left operand");
        ok = require_scalar(n->u.binary.rhs, b, "right operand") && ok;
        return ok ? T_INT : T_ERROR;
    }
    case N_ASSIGN: {
        Type a = check_expr(n->u.binary.lhs);
        Type b = check_expr(n->u.binary.rhs);
        if (a == T_ERROR || b == T_ERROR)
            return T_ERROR;
        if (!is_lvalue(n->u.binary.lhs)) {
            diag_error(n->line, "expression is not assignable");
            return T_ERROR;
        }
        if (!require_scalar(n->u.binary.rhs, b, "assigned value"))
            return T_ERROR;
        if (n->u.binary.op == OP_MOD && (a != T_INT || b != T_INT)) {
            diag_error(n->line, "invalid operands to '%%=' ('%s' and '%s')", type_name(a), type_name(b));
            return T_ERROR;
        }
        return a;
    }
    case N_TERNARY: {
        Type c = check_expr(n->u.branch.cond);
        Type a = check_expr(n->u.branch.then);
        Type b = check_expr(n->u.branch.els);
        if (a == T_STRING && b == T_STRING)
            return require_scalar(n->u.branch.cond, c, "condition") ? T_STRING : T_ERROR;
        int ok = require_scalar(n->u.branch.cond, c, "condition");
        ok = require_scalar(n->u.branch.then, a, "operand") && ok;
        ok = require_scalar(n->u.branch.els, b, "operand") && ok;
        return ok ? arith(a, b) : T_ERROR;
    }
    default:
        return T_ERROR;
    }
}

static Type check_expr(Node *n)
{
    n->type = check_expr_inner(n);
    return n->type;
}

static void check_cond(Node *n)
{
    if (n)
        require_scalar(n, check_expr(n), "condition");
}

static void check_var(Node *v)
{
    Type t = v->u.var.type;
    if (t == T_VOID || t == T_ERROR) {
        diag_error(v->line, "variable '%s' declared void", v->u.var.name);
        v->u.var.type = T_INT;
    }
    Node *init = v->u.var.init;
    if (init) {
        Type it = check_expr(init);
        if (is_array(v->u.var.type))
            diag_error(v->line, "array initializers are not supported");
        else
            require_scalar(init, it, "initializer");
        if (!scope->parent && !is_constant(init))
            diag_error(init->line, "initializer of global '%s' is not a compile-time constant", v->u.var.name);
    }
    declare(v);
}

static void check_items(NodeList *items)
{
    for (int i = 0; i < items->len; i++)
        check_stmt(items->items[i]);
}

static void check_stmt(Node *n)
{
    if (!n)
        return;
    switch (n->kind) {
    case N_DECL:
        for (int i = 0; i < n->u.decl.vars->len; i++)
            check_var(n->u.decl.vars->items[i]);
        break;
    case N_BLOCK:
        scope_push();
        check_items(n->u.list.items);
        scope_pop();
        break;
    case N_IF:
        check_cond(n->u.branch.cond);
        check_stmt(n->u.branch.then);
        check_stmt(n->u.branch.els);
        break;
    case N_WHILE:
    case N_DO_WHILE:
    case N_FOR:
        scope_push();
        check_stmt(n->u.loop.init);
        check_cond(n->u.loop.cond);
        if (n->u.loop.step)
            check_expr(n->u.loop.step);
        loop_depth++;
        check_stmt(n->u.loop.body);
        loop_depth--;
        scope_pop();
        break;
    case N_RETURN: {
        Type ret = current_fn->u.func.ret;
        Node *e = n->u.wrap.expr;
        if (e) {
            Type t = check_expr(e);
            if (ret == T_VOID)
                diag_error(n->line, "void function '%s' should not return a value", current_fn->u.func.name);
            else
                require_scalar(e, t, "return value");
        } else if (ret != T_VOID) {
            diag_error(n->line, "non-void function '%s' should return a value", current_fn->u.func.name);
        }
        break;
    }
    case N_BREAK:
    case N_CONTINUE:
        if (!loop_depth)
            diag_error(n->line, "'%s' statement not in loop", n->kind == N_BREAK ? "break" : "continue");
        break;
    case N_EXPR_STMT:
        check_expr(n->u.wrap.expr);
        break;
    default:
        break;
    }
}

static int same_signature(Node *a, Node *b)
{
    NodeList *pa = a->u.func.params;
    NodeList *pb = b->u.func.params;
    if (a->u.func.ret != b->u.func.ret || pa->len != pb->len)
        return 0;
    for (int i = 0; i < pa->len; i++)
        if (pa->items[i]->u.var.type != pb->items[i]->u.var.type)
            return 0;
    return 1;
}

static void register_func(Node *fn)
{
    const char *name = fn->u.func.name;
    if (scope_find(scope, name)) {
        diag_error(fn->line, "'%s' redeclared as a different kind of symbol", name);
        return;
    }
    FuncEntry *fe = find_func(name);
    if (fe) {
        if (!same_signature(fe->decl, fn))
            diag_error(fn->line, "conflicting types for '%s'", name);
        if (fn->u.func.body) {
            if (fe->def)
                diag_error(fn->line, "redefinition of function '%s'", name);
            fe->def = fn;
            fe->decl->u.func.def = fn;
            fn->u.func.def = fn;
        }
        return;
    }
    funcs = realloc(funcs, sizeof(FuncEntry) * (nfuncs + 1));
    fe = &funcs[nfuncs++];
    fe->name = fn->u.func.name;
    fe->decl = fn;
    fe->def = fn->u.func.body ? fn : NULL;
    fe->used_line = 0;
    fn->u.func.def = fe->def;
}

static void check_func(Node *fn)
{
    for (int i = 0; i < fn->u.func.params->len; i++) {
        Node *p = fn->u.func.params->items[i];
        if (p->u.var.type == T_VOID || p->u.var.type == T_ERROR) {
            diag_error(p->line, "parameter '%s' has void type", p->u.var.name);
            p->u.var.type = T_INT;
        }
    }
    register_func(fn);
    if (!fn->u.func.body)
        return;

    current_fn = fn;
    next_slot = 0;
    max_slot = 0;
    loop_depth = 0;
    scope_push();
    for (int i = 0; i < fn->u.func.params->len; i++)
        declare(fn->u.func.params->items[i]);
    check_items(fn->u.func.body->u.list.items);
    scope_pop();
    fn->u.func.nlocals = max_slot;
    current_fn = NULL;
}

int sema_analyze(Node *root, Program *prog)
{
    int before = diag_count();
    scope = NULL;
    funcs = NULL;
    nfuncs = 0;
    nglobals = 0;
    scope_push();

    NodeList *items = root->u.list.items;
    for (int i = 0; i < items->len; i++) {
        Node *n = items->items[i];
        if (n->kind == N_FUNC)
            check_func(n);
        else
            check_stmt(n);
    }

    for (int i = 0; i < nfuncs; i++) {
        if (!funcs[i].def && funcs[i].used_line)
            diag_error(funcs[i].used_line, "undefined reference to '%s'", funcs[i].name);
    }

    FuncEntry *m = find_func("main");
    prog->main_fn = m ? m->def : NULL;
    prog->nglobals = nglobals;
    if (!prog->main_fn)
        diag_error(root->line, "no definition of 'main'");
    else if (prog->main_fn->u.func.ret != T_INT || prog->main_fn->u.func.params->len != 0)
        diag_error(prog->main_fn->line, "'main' must be declared as 'int main(void)'");

    scope_pop();
    free(funcs);
    funcs = NULL;
    return diag_count() == before;
}
