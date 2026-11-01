#include "interp.h"

#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diag.h"
#include "format.h"

#define MAX_DEPTH 20000

typedef struct Array Array;

typedef struct {
    Type type;
    union {
        int i;
        double d;
        Array *a;
        const char *s;
    } as;
} Value;

struct Array {
    Type elem;
    int len;
    Value *data;
};

typedef struct {
    Value *slots;
    Array **owned;
    int nowned;
    int cap;
} Frame;

typedef enum {
    FLOW_NORMAL,
    FLOW_BREAK,
    FLOW_CONTINUE,
    FLOW_RETURN
} Flow;

static Value *globals;
static Frame global_frame;
static Frame *frame;
static Value ret_val;
static int depth;
static jmp_buf on_error;

static Value eval(Node *n);
static Flow exec(Node *n);

static void runtime_error(int line, const char *fmt, ...)
{
    va_list ap;
    fflush(stdout);
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d: runtime error: ", diag_file(), line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    longjmp(on_error, 1);
}

static Value make_int(int i)
{
    Value v;
    v.type = T_INT;
    v.as.i = i;
    return v;
}

static Value make_double(double d)
{
    Value v;
    v.type = T_DOUBLE;
    v.as.d = d;
    return v;
}

static int to_int(Value v)
{
    return v.type == T_DOUBLE ? (int)v.as.d : v.as.i;
}

static double to_double(Value v)
{
    return v.type == T_DOUBLE ? v.as.d : (double)v.as.i;
}

static Value convert(Value v, Type to)
{
    if (to == T_INT)
        return make_int(to_int(v));
    if (to == T_DOUBLE)
        return make_double(to_double(v));
    return v;
}

static int truthy(Value v)
{
    return v.type == T_DOUBLE ? v.as.d != 0.0 : v.as.i != 0;
}

static int wrap_add(int a, int b) { return (int)((unsigned)a + (unsigned)b); }
static int wrap_sub(int a, int b) { return (int)((unsigned)a - (unsigned)b); }
static int wrap_mul(int a, int b) { return (int)((unsigned)a * (unsigned)b); }

static int int_div(int a, int b, int line)
{
    if (b == 0)
        runtime_error(line, "integer division by zero");
    if (a == INT_MIN && b == -1)
        return INT_MIN;
    return a / b;
}

static int int_mod(int a, int b, int line)
{
    if (b == 0)
        runtime_error(line, "integer remainder by zero");
    if (b == -1)
        return 0;
    return a % b;
}

static Value arith(Op op, Value a, Value b, Type type, int line)
{
    if (type == T_DOUBLE) {
        double x = to_double(a), y = to_double(b);
        switch (op) {
        case OP_ADD: return make_double(x + y);
        case OP_SUB: return make_double(x - y);
        case OP_MUL: return make_double(x * y);
        case OP_DIV: return make_double(x / y);
        default: break;
        }
    } else {
        int x = to_int(a), y = to_int(b);
        switch (op) {
        case OP_ADD: return make_int(wrap_add(x, y));
        case OP_SUB: return make_int(wrap_sub(x, y));
        case OP_MUL: return make_int(wrap_mul(x, y));
        case OP_DIV: return make_int(int_div(x, y, line));
        case OP_MOD: return make_int(int_mod(x, y, line));
        default: break;
        }
    }
    runtime_error(line, "bad arithmetic operator");
    return make_int(0);
}

static Value compare(Op op, Value a, Value b)
{
    double x = to_double(a), y = to_double(b);
    int r = 0;
    if (a.type == T_INT && b.type == T_INT) {
        int p = a.as.i, q = b.as.i;
        switch (op) {
        case OP_LT: r = p < q; break;
        case OP_GT: r = p > q; break;
        case OP_LE: r = p <= q; break;
        case OP_GE: r = p >= q; break;
        case OP_EQ: r = p == q; break;
        default: r = p != q; break;
        }
        return make_int(r);
    }
    switch (op) {
    case OP_LT: r = x < y; break;
    case OP_GT: r = x > y; break;
    case OP_LE: r = x <= y; break;
    case OP_GE: r = x >= y; break;
    case OP_EQ: r = x == y; break;
    default: r = x != y; break;
    }
    return make_int(r);
}

static Array *array_new(Type elem, int len)
{
    Array *a = malloc(sizeof(Array));
    a->elem = elem;
    a->len = len;
    a->data = calloc((size_t)len, sizeof(Value));
    for (int i = 0; i < len; i++)
        a->data[i] = elem == T_DOUBLE ? make_double(0.0) : make_int(0);
    if (frame->nowned == frame->cap) {
        frame->cap = frame->cap ? frame->cap * 2 : 4;
        frame->owned = realloc(frame->owned, sizeof(Array *) * frame->cap);
    }
    frame->owned[frame->nowned++] = a;
    return a;
}

static void frame_release(Frame *f)
{
    for (int i = 0; i < f->nowned; i++) {
        free(f->owned[i]->data);
        free(f->owned[i]);
    }
    free(f->owned);
    free(f->slots);
}

static Value *slot_of(Symbol *sym)
{
    return sym->global ? &globals[sym->slot] : &frame->slots[sym->slot];
}

static Value *lvalue(Node *n)
{
    if (n->kind == N_IDENT)
        return slot_of(n->u.ident.sym);
    Array *a = eval(n->u.index.base).as.a;
    int i = to_int(eval(n->u.index.index));
    if (i < 0 || i >= a->len)
        runtime_error(n->line, "index %d out of bounds for array of length %d", i, a->len);
    return &a->data[i];
}

static void do_printf(Node *n)
{
    NodeList *args = n->u.call.args;
    const char *fmt = args->items[0]->u.slit.value;
    const char *p;
    FormatSpec spec;
    int arg = 1;
    char piece[64];
    char out[512];

    while ((p = format_next(fmt, &spec)) != NULL) {
        fwrite(fmt, 1, (size_t)(spec.start - fmt), stdout);
        fmt = p;
        if (spec.conv == '%') {
            fputc('%', stdout);
            continue;
        }
        int len = spec.len < (int)sizeof piece ? spec.len : (int)sizeof piece - 1;
        memcpy(piece, spec.start, (size_t)len);
        piece[len] = '\0';
        Node *a = args->items[arg++];
        switch (format_arg_type(spec.conv)) {
        case T_INT:
            snprintf(out, sizeof out, piece, to_int(eval(a)));
            break;
        case T_DOUBLE:
            snprintf(out, sizeof out, piece, to_double(eval(a)));
            break;
        default:
            snprintf(out, sizeof out, piece, eval(a).as.s);
            break;
        }
        fputs(out, stdout);
    }
    fputs(fmt, stdout);
}

static Value call(Node *n)
{
    NodeList *args = n->u.call.args;
    switch (n->u.call.builtin) {
    case BI_PRINTF:
        do_printf(n);
        return make_int(0);
    case BI_SQRT:
        return make_double(sqrt(to_double(eval(args->items[0]))));
    case BI_FABS:
        return make_double(fabs(to_double(eval(args->items[0]))));
    case BI_POW: {
        double x = to_double(eval(args->items[0]));
        return make_double(pow(x, to_double(eval(args->items[1]))));
    }
    default:
        break;
    }

    Node *fn = n->u.call.callee->u.func.def;
    NodeList *params = fn->u.func.params;
    Value *slots = calloc((size_t)(fn->u.func.nlocals ? fn->u.func.nlocals : 1), sizeof(Value));
    for (int i = 0; i < params->len; i++)
        slots[i] = convert(eval(args->items[i]), params->items[i]->u.var.type);

    if (++depth > MAX_DEPTH)
        runtime_error(n->line, "stack overflow: recursion deeper than %d calls", MAX_DEPTH);

    Frame f = { slots, NULL, 0, 0 };
    Frame *saved = frame;
    frame = &f;
    Flow flow = exec(fn->u.func.body);
    frame = saved;
    depth--;
    frame_release(&f);

    if (fn->u.func.ret == T_VOID)
        return make_int(0);
    if (flow != FLOW_RETURN)
        return convert(make_int(0), fn->u.func.ret);
    return convert(ret_val, fn->u.func.ret);
}

static Value eval(Node *n)
{
    switch (n->kind) {
    case N_INT_LIT:
        return make_int((int)n->u.ilit.value);
    case N_DOUBLE_LIT:
        return make_double(n->u.dlit.value);
    case N_STRING_LIT: {
        Value v;
        v.type = T_STRING;
        v.as.s = n->u.slit.value;
        return v;
    }
    case N_IDENT:
        return *slot_of(n->u.ident.sym);
    case N_INDEX:
        return *lvalue(n);
    case N_CALL:
        return call(n);
    case N_CAST:
        return convert(eval(n->u.unary.operand), n->type);
    case N_UNARY: {
        Value v = eval(n->u.unary.operand);
        switch (n->u.unary.op) {
        case OP_NEG:
            return v.type == T_DOUBLE ? make_double(-v.as.d) : make_int(wrap_sub(0, v.as.i));
        case OP_NOT:
            return make_int(!truthy(v));
        default:
            return v;
        }
    }
    case N_INCDEC: {
        Value *ref = lvalue(n->u.unary.operand);
        Value old = *ref;
        int inc = n->u.unary.op == OP_PRE_INC || n->u.unary.op == OP_POST_INC;
        *ref = arith(inc ? OP_ADD : OP_SUB, old, make_int(1), old.type, n->line);
        return n->u.unary.op == OP_PRE_INC || n->u.unary.op == OP_PRE_DEC ? *ref : old;
    }
    case N_BINARY: {
        Value a = eval(n->u.binary.lhs);
        Value b = eval(n->u.binary.rhs);
        switch (n->u.binary.op) {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            return arith(n->u.binary.op, a, b, n->type, n->line);
        default:
            return compare(n->u.binary.op, a, b);
        }
    }
    case N_LOGICAL: {
        int a = truthy(eval(n->u.binary.lhs));
        if (n->u.binary.op == OP_AND)
            return make_int(a && truthy(eval(n->u.binary.rhs)));
        return make_int(a || truthy(eval(n->u.binary.rhs)));
    }
    case N_ASSIGN: {
        Value rhs = eval(n->u.binary.rhs);
        Value *ref = lvalue(n->u.binary.lhs);
        if (n->u.binary.op != OP_NONE) {
            Type t = n->u.binary.op == OP_MOD ? T_INT : (ref->type == T_DOUBLE || rhs.type == T_DOUBLE ? T_DOUBLE : T_INT);
            rhs = arith(n->u.binary.op, *ref, rhs, t, n->line);
        }
        *ref = convert(rhs, n->type);
        return *ref;
    }
    case N_TERNARY:
        return convert(eval(truthy(eval(n->u.branch.cond)) ? n->u.branch.then : n->u.branch.els), n->type);
    default:
        runtime_error(n->line, "cannot evaluate %s", kind_name(n->kind));
    }
    return make_int(0);
}

static void declare_var(Node *v)
{
    Value *slot = slot_of(v->u.var.sym);
    Type t = v->u.var.type;
    if (is_array(t)) {
        slot->type = t;
        slot->as.a = array_new(elem_type(t), v->u.var.size);
    } else if (v->u.var.init) {
        *slot = convert(eval(v->u.var.init), t);
    } else {
        *slot = convert(make_int(0), t);
    }
}

static Flow exec_loop(Node *n)
{
    if (n->u.loop.init)
        exec(n->u.loop.init);
    int first = n->kind == N_DO_WHILE;
    for (;;) {
        if (!first && n->u.loop.cond && !truthy(eval(n->u.loop.cond)))
            break;
        first = 0;
        Flow f = exec(n->u.loop.body);
        if (f == FLOW_BREAK)
            break;
        if (f == FLOW_RETURN)
            return f;
        if (n->u.loop.step)
            eval(n->u.loop.step);
    }
    return FLOW_NORMAL;
}

static Flow exec(Node *n)
{
    if (!n)
        return FLOW_NORMAL;
    switch (n->kind) {
    case N_DECL:
        for (int i = 0; i < n->u.decl.vars->len; i++)
            declare_var(n->u.decl.vars->items[i]);
        return FLOW_NORMAL;
    case N_BLOCK:
        for (int i = 0; i < n->u.list.items->len; i++) {
            Flow f = exec(n->u.list.items->items[i]);
            if (f != FLOW_NORMAL)
                return f;
        }
        return FLOW_NORMAL;
    case N_IF:
        if (truthy(eval(n->u.branch.cond)))
            return exec(n->u.branch.then);
        return exec(n->u.branch.els);
    case N_WHILE:
    case N_DO_WHILE:
    case N_FOR:
        return exec_loop(n);
    case N_RETURN:
        if (n->u.wrap.expr)
            ret_val = eval(n->u.wrap.expr);
        return FLOW_RETURN;
    case N_BREAK:
        return FLOW_BREAK;
    case N_CONTINUE:
        return FLOW_CONTINUE;
    case N_EXPR_STMT:
        eval(n->u.wrap.expr);
        return FLOW_NORMAL;
    default:
        return FLOW_NORMAL;
    }
}

int interp_run(Node *root, const Program *prog, int *exit_code)
{
    volatile int ok = 1;
    globals = calloc((size_t)(prog->nglobals ? prog->nglobals : 1), sizeof(Value));
    memset(&global_frame, 0, sizeof global_frame);
    frame = &global_frame;
    depth = 0;

    if (setjmp(on_error) == 0) {
        NodeList *items = root->u.list.items;
        for (int i = 0; i < items->len; i++)
            if (items->items[i]->kind == N_DECL)
                exec(items->items[i]);
        Node main_call;
        memset(&main_call, 0, sizeof main_call);
        main_call.kind = N_CALL;
        main_call.line = prog->main_fn->line;
        main_call.u.call.callee = prog->main_fn;
        main_call.u.call.args = list_new();
        *exit_code = call(&main_call).as.i;
        free(main_call.u.call.args);
    } else {
        ok = 0;
        *exit_code = 1;
    }
    fflush(stdout);
    frame_release(&global_frame);
    free(globals);
    return ok;
}
