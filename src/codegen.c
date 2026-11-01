#include "codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

enum {
    PREC_ASSIGN = 2,
    PREC_COND = 3,
    PREC_OR = 4,
    PREC_AND = 5,
    PREC_BITOR = 6,
    PREC_EQ = 9,
    PREC_REL = 10,
    PREC_ADD = 12,
    PREC_MUL = 13,
    PREC_UNARY = 15,
    PREC_POSTFIX = 16,
    PREC_CALL = 18,
    PREC_ATOM = 20
};

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static Buf *out;
static int depth;
static int uses_idiv, uses_imod, uses_toint, uses_printf;
static Type cur_ret;

static void emit_expr(const Node *n, int ctx);
static void emit_cond(const Node *n, int ctx);
static void emit_stmt(const Node *n);
static void emit_stmt_inline(const Node *n);

static const char *const runtime_printf[] = {
    "function __pad(s, width, left, zero) {\n",
    "    if (s.length >= width) return s;\n",
    "    if (left) return s + \" \".repeat(width - s.length);\n",
    "    if (zero) {\n",
    "        const sign = /^[+\\- ]/.test(s) ? s[0] : \"\";\n",
    "        const prefix = /^[+\\- ]?0[xX]/.test(s) ? s.slice(0, sign.length + 2) : sign;\n",
    "        return prefix + \"0\".repeat(width - s.length) + s.slice(prefix.length);\n",
    "    }\n",
    "    return \" \".repeat(width - s.length) + s;\n",
    "}\n",
    "function __tie(full, extra) {\n",
    "    const tail = full.slice(full.length - extra);\n",
    "    if (tail !== \"5\" + \"0\".repeat(extra - 1)) return null;\n",
    "    let kept = full.slice(0, full.length - extra);\n",
    "    if (kept.endsWith(\".\")) kept = kept.slice(0, -1);\n",
    "    return Number(kept[kept.length - 1]) % 2 === 0 ? kept : null;\n",
    "}\n",
    "function __fixed(x, p) {\n",
    "    if (p > 80) return x.toFixed(p);\n",
    "    return __tie(x.toFixed(p + 20), 20) ?? x.toFixed(p);\n",
    "}\n",
    "function __exp(x, p, upper) {\n",
    "    let m, e;\n",
    "    if (p <= 80) {\n",
    "        const [fm, fe] = x.toExponential(p + 20).split(\"e\");\n",
    "        const t = __tie(fm, 20);\n",
    "        if (t !== null) [m, e] = [t, fe];\n",
    "    }\n",
    "    if (m === undefined) [m, e] = x.toExponential(p).split(\"e\");\n",
    "    const sign = e[0] === \"-\" ? \"-\" : \"+\";\n",
    "    e = e.replace(/^[+-]/, \"\");\n",
    "    if (e.length < 2) e = \"0\" + e;\n",
    "    const r = m + \"e\" + sign + e;\n",
    "    return upper ? r.toUpperCase() : r;\n",
    "}\n",
    "function __fmtg(x, p, alt, upper) {\n",
    "    if (p === 0) p = 1;\n",
    "    if (x === 0) return alt ? __fixed(0, p - 1) : \"0\";\n",
    "    const e = Number(x.toExponential(p - 1).split(\"e\")[1]);\n",
    "    let s = e < -4 || e >= p ? __exp(x, p - 1, upper) : __fixed(x, p - 1 - e);\n",
    "    if (!alt) {\n",
    "        s = s.replace(/(\\.\\d*?)0+(e|E|$)/, \"$1$2\").replace(/\\.(e|E|$)/, \"$1\");\n",
    "    }\n",
    "    return s;\n",
    "}\n",
    "function __printf(fmt, ...args) {\n",
    "    let i = 0;\n",
    "    const out = fmt.replace(/%([-+ 0#]*)(\\d*)(?:\\.(\\d*))?[lh]*([diouxXcfFeEgGs%])/g, (_, flags, w, prec, conv) => {\n",
    "        if (conv === \"%\") return \"%\";\n",
    "        const v = args[i++];\n",
    "        const left = flags.includes(\"-\");\n",
    "        const zero = flags.includes(\"0\") && !left;\n",
    "        const plus = flags.includes(\"+\");\n",
    "        const space = flags.includes(\" \");\n",
    "        const alt = flags.includes(\"#\");\n",
    "        const width = w ? Number(w) : 0;\n",
    "        const p = prec === undefined ? -1 : Number(prec || 0);\n",
    "        let s;\n",
    "        let numeric = true;\n",
    "        switch (conv) {\n",
    "        case \"d\": case \"i\": {\n",
    "            const n = v | 0;\n",
    "            s = Math.abs(n).toString();\n",
    "            if (p >= 0) s = p === 0 && n === 0 ? \"\" : s.padStart(p, \"0\");\n",
    "            s = (n < 0 ? \"-\" : plus ? \"+\" : space ? \" \" : \"\") + s;\n",
    "            break;\n",
    "        }\n",
    "        case \"u\": case \"o\": case \"x\": case \"X\": {\n",
    "            const n = v >>> 0;\n",
    "            s = n.toString(conv === \"u\" ? 10 : conv === \"o\" ? 8 : 16);\n",
    "            if (conv === \"X\") s = s.toUpperCase();\n",
    "            if (p >= 0) s = p === 0 && n === 0 ? \"\" : s.padStart(p, \"0\");\n",
    "            if (alt && n !== 0) s = (conv === \"o\" ? \"0\" : conv === \"x\" ? \"0x\" : conv === \"X\" ? \"0X\" : \"\") + s;\n",
    "            break;\n",
    "        }\n",
    "        case \"c\":\n",
    "            s = String.fromCharCode(v & 255);\n",
    "            numeric = false;\n",
    "            break;\n",
    "        case \"s\":\n",
    "            s = p >= 0 ? String(v).slice(0, p) : String(v);\n",
    "            numeric = false;\n",
    "            break;\n",
    "        default: {\n",
    "            const q = p < 0 ? 6 : p;\n",
    "            const neg = v < 0 || Object.is(v, -0);\n",
    "            const a = Math.abs(v);\n",
    "            if (!isFinite(a)) {\n",
    "                s = isNaN(a) ? \"nan\" : \"inf\";\n",
    "                if (conv === \"F\" || conv === \"E\" || conv === \"G\") s = s.toUpperCase();\n",
    "            } else if (conv === \"f\" || conv === \"F\") {\n",
    "                s = __fixed(a, q);\n",
    "                if (alt && q === 0) s += \".\";\n",
    "            } else if (conv === \"e\" || conv === \"E\") {\n",
    "                s = __exp(a, q, conv === \"E\");\n",
    "            } else {\n",
    "                s = __fmtg(a, q, alt, conv === \"G\");\n",
    "            }\n",
    "            s = (neg ? \"-\" : plus ? \"+\" : space ? \" \" : \"\") + s;\n",
    "            if (!isFinite(a)) numeric = false;\n",
    "        }\n",
    "        }\n",
    "        return __pad(s, width, left, zero && numeric && (p < 0 || \"fFeEgG\".includes(conv)));\n",
    "    });\n",
    "    __write(out);\n",
    "    return out.length;\n",
    "}\n",
    NULL
};

static void put(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list cp;
    va_copy(cp, ap);
    int need = vsnprintf(NULL, 0, fmt, cp);
    va_end(cp);
    if (out->len + (size_t)need + 1 > out->cap) {
        while (out->len + (size_t)need + 1 > out->cap)
            out->cap = out->cap ? out->cap * 2 : 1024;
        out->data = realloc(out->data, out->cap);
    }
    vsnprintf(out->data + out->len, out->cap - out->len, fmt, ap);
    out->len += (size_t)need;
    va_end(ap);
}

static void put_js_string(const char *s)
{
    put("\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': put("\\\""); break;
        case '\\': put("\\\\"); break;
        case '\n': put("\\n"); break;
        case '\t': put("\\t"); break;
        case '\r': put("\\r"); break;
        default:
            if (*p < 0x20)
                put("\\x%02x", *p);
            else
                put("%c", *p);
        }
    }
    put("\"");
}

static void indent(void)
{
    for (int i = 0; i < depth; i++)
        put("    ");
}

static const char *js_name(const char *name)
{
    static const char *const reserved[] = {
        "arguments", "await", "case", "catch", "class", "const", "debugger", "default", "delete",
        "enum", "eval", "export", "extends", "false", "finally", "function", "implements", "import",
        "in", "instanceof", "interface", "let", "new", "null", "package", "private", "protected",
        "public", "static", "super", "switch", "this", "throw", "true", "try", "typeof", "var",
        "with", "yield", "undefined", "NaN", "Infinity", "Math", "Object", "Array", "Number",
        "String", "process", "globalThis", "console", "Int32Array", "Float64Array", NULL
    };
    static char buf[256];
    for (const char *const *r = reserved; *r; r++) {
        if (strcmp(*r, name) == 0) {
            snprintf(buf, sizeof buf, "%s_", name);
            return buf;
        }
    }
    return name;
}

static int open_paren(int prec, int ctx)
{
    if (prec < ctx) {
        put("(");
        return 1;
    }
    return 0;
}

static void close_paren(int opened)
{
    if (opened)
        put(")");
}

static void emit_converted(const Node *n, Type to, int ctx)
{
    if (to == T_INT && n->type == T_DOUBLE) {
        uses_toint = 1;
        put("__toint(");
        emit_expr(n, PREC_ASSIGN);
        put(")");
    } else {
        emit_expr(n, ctx);
    }
}

static void emit_arith(Op op, const Node *l, const Node *r, Type type, int ctx)
{
    if (type == T_DOUBLE) {
        int prec = op == OP_ADD || op == OP_SUB ? PREC_ADD : PREC_MUL;
        int p = open_paren(prec, ctx);
        emit_expr(l, prec);
        put(" %s ", op_symbol(op));
        emit_expr(r, prec + 1);
        close_paren(p);
        return;
    }
    switch (op) {
    case OP_ADD:
    case OP_SUB: {
        int p = open_paren(PREC_BITOR, ctx);
        put("(");
        emit_expr(l, PREC_ADD);
        put(" %s ", op_symbol(op));
        emit_expr(r, PREC_ADD + 1);
        put(") | 0");
        close_paren(p);
        return;
    }
    case OP_MUL:
        put("Math.imul(");
        break;
    case OP_DIV:
        uses_idiv = 1;
        put("__idiv(");
        break;
    default:
        uses_imod = 1;
        put("__imod(");
        break;
    }
    emit_converted(l, T_INT, PREC_ASSIGN);
    put(", ");
    emit_converted(r, T_INT, PREC_ASSIGN);
    put(")");
}

static int is_comparison(const Node *n)
{
    return n->kind == N_BINARY && n->u.binary.op >= OP_LT && n->u.binary.op <= OP_NE;
}

static void emit_comparison(const Node *n, int ctx)
{
    int prec = n->u.binary.op == OP_EQ || n->u.binary.op == OP_NE ? PREC_EQ : PREC_REL;
    const char *sym = n->u.binary.op == OP_EQ ? "===" : n->u.binary.op == OP_NE ? "!==" : op_symbol(n->u.binary.op);
    int p = open_paren(prec, ctx);
    emit_expr(n->u.binary.lhs, prec);
    put(" %s ", sym);
    emit_expr(n->u.binary.rhs, prec + 1);
    close_paren(p);
}

static void emit_logical(const Node *n, int ctx)
{
    int prec = n->u.binary.op == OP_AND ? PREC_AND : PREC_OR;
    int p = open_paren(prec, ctx);
    emit_cond(n->u.binary.lhs, prec);
    put(" %s ", op_symbol(n->u.binary.op));
    emit_cond(n->u.binary.rhs, prec + 1);
    close_paren(p);
}

static void emit_cond(const Node *n, int ctx)
{
    if (is_comparison(n)) {
        emit_comparison(n, ctx);
    } else if (n->kind == N_LOGICAL) {
        emit_logical(n, ctx);
    } else if (n->kind == N_UNARY && n->u.unary.op == OP_NOT) {
        int p = open_paren(PREC_UNARY, ctx);
        put("!");
        emit_cond(n->u.unary.operand, PREC_UNARY);
        close_paren(p);
    } else {
        emit_expr(n, ctx);
    }
}

static int compound_is_native(const Node *n)
{
    Type target = n->u.binary.lhs->type;
    Type rhs = n->u.binary.rhs->type;
    Op op = n->u.binary.op;
    if (target == T_DOUBLE)
        return 1;
    if (n->u.binary.lhs->kind != N_INDEX)
        return 0;
    if (rhs == T_DOUBLE)
        return op != OP_MOD;
    return op == OP_ADD || op == OP_SUB;
}

static void emit_assign(const Node *n, int ctx)
{
    const Node *lhs = n->u.binary.lhs;
    const Node *rhs = n->u.binary.rhs;
    Op op = n->u.binary.op;
    int p = open_paren(PREC_ASSIGN, ctx);
    emit_expr(lhs, PREC_POSTFIX);

    if (op == OP_NONE) {
        put(" = ");
        if (lhs->kind == N_INDEX)
            emit_expr(rhs, PREC_ASSIGN);
        else
            emit_converted(rhs, lhs->type, PREC_ASSIGN);
    } else if (compound_is_native(n)) {
        put(" %s= ", op_symbol(op));
        emit_expr(rhs, PREC_ASSIGN);
    } else if (rhs->type == T_DOUBLE) {
        uses_toint = 1;
        put(" = __toint(");
        emit_arith(op, lhs, rhs, T_DOUBLE, PREC_ASSIGN);
        put(")");
    } else {
        put(" = ");
        emit_arith(op, lhs, rhs, T_INT, PREC_ASSIGN);
    }
    close_paren(p);
}

static void emit_call(const Node *n)
{
    NodeList *args = n->u.call.args;
    switch (n->u.call.builtin) {
    case BI_PRINTF:
        uses_printf = 1;
        put("__printf(");
        for (int i = 0; i < args->len; i++) {
            if (i)
                put(", ");
            if (args->items[i]->kind == N_STRING_LIT)
                put_js_string(args->items[i]->u.slit.value);
            else
                emit_expr(args->items[i], PREC_ASSIGN);
        }
        put(")");
        return;
    case BI_SQRT: put("Math.sqrt("); break;
    case BI_FABS: put("Math.abs("); break;
    case BI_POW: put("Math.pow("); break;
    default: put("%s(", js_name(n->u.call.name)); break;
    }
    NodeList *params = n->u.call.callee ? n->u.call.callee->u.func.params : NULL;
    for (int i = 0; i < args->len; i++) {
        if (i)
            put(", ");
        Type want = params ? params->items[i]->u.var.type : T_DOUBLE;
        emit_converted(args->items[i], want, PREC_ASSIGN);
    }
    put(")");
}

static void emit_expr(const Node *n, int ctx)
{
    int p;
    switch (n->kind) {
    case N_INT_LIT:
        p = open_paren(n->u.ilit.value < 0 ? PREC_UNARY : PREC_ATOM, ctx);
        put("%ld", n->u.ilit.value);
        close_paren(p);
        break;
    case N_DOUBLE_LIT:
        put("%s", n->u.dlit.text);
        break;
    case N_STRING_LIT:
        put_js_string(n->u.slit.value);
        break;
    case N_IDENT:
        put("%s", js_name(n->u.ident.name));
        break;
    case N_INDEX:
        emit_expr(n->u.index.base, PREC_CALL);
        put("[");
        emit_expr(n->u.index.index, PREC_ASSIGN);
        put("]");
        break;
    case N_CALL:
        emit_call(n);
        break;
    case N_CAST:
        if (n->type == T_INT && n->u.unary.operand->type == T_DOUBLE)
            emit_converted(n->u.unary.operand, T_INT, ctx);
        else
            emit_expr(n->u.unary.operand, ctx);
        break;
    case N_UNARY:
        switch (n->u.unary.op) {
        case OP_NOT:
            p = open_paren(PREC_UNARY, ctx);
            put("+!");
            emit_cond(n->u.unary.operand, PREC_UNARY);
            close_paren(p);
            break;
        case OP_NEG:
            if (n->type == T_INT && n->u.unary.operand->kind != N_INT_LIT) {
                p = open_paren(PREC_BITOR, ctx);
                put("-");
                emit_expr(n->u.unary.operand, PREC_UNARY);
                put(" | 0");
            } else {
                p = open_paren(PREC_UNARY, ctx);
                put("-");
                emit_expr(n->u.unary.operand, PREC_UNARY);
            }
            close_paren(p);
            break;
        default:
            emit_expr(n->u.unary.operand, ctx);
            break;
        }
        break;
    case N_INCDEC: {
        int post = n->u.unary.op == OP_POST_INC || n->u.unary.op == OP_POST_DEC;
        p = open_paren(post ? PREC_POSTFIX : PREC_UNARY, ctx);
        if (!post)
            put("%s", op_symbol(n->u.unary.op));
        emit_expr(n->u.unary.operand, PREC_POSTFIX);
        if (post)
            put("%s", op_symbol(n->u.unary.op));
        close_paren(p);
        break;
    }
    case N_BINARY:
        if (is_comparison(n)) {
            p = open_paren(PREC_UNARY, ctx);
            put("+(");
            emit_comparison(n, PREC_ASSIGN);
            put(")");
            close_paren(p);
        } else {
            emit_arith(n->u.binary.op, n->u.binary.lhs, n->u.binary.rhs, n->type, ctx);
        }
        break;
    case N_LOGICAL:
        p = open_paren(PREC_COND, ctx);
        emit_logical(n, PREC_OR);
        put(" ? 1 : 0");
        close_paren(p);
        break;
    case N_ASSIGN:
        emit_assign(n, ctx);
        break;
    case N_TERNARY:
        p = open_paren(PREC_COND, ctx);
        emit_cond(n->u.branch.cond, PREC_OR);
        put(" ? ");
        emit_converted(n->u.branch.then, n->type, PREC_ASSIGN);
        put(" : ");
        emit_converted(n->u.branch.els, n->type, PREC_ASSIGN);
        close_paren(p);
        break;
    default:
        put("undefined");
        break;
    }
}

static void emit_var_value(const Node *v)
{
    Type t = v->u.var.type;
    if (is_array(t))
        put("new %s(%d)", t == T_INT_ARRAY ? "Int32Array" : "Float64Array", v->u.var.size);
    else if (v->u.var.init)
        emit_converted(v->u.var.init, t, PREC_ASSIGN);
    else
        put("0");
}

static void emit_var(const Node *v)
{
    put("let %s = ", js_name(v->u.var.name));
    emit_var_value(v);
}

static void emit_body(const Node *n)
{
    if (n->kind == N_BLOCK) {
        emit_stmt_inline(n);
        return;
    }
    put("{\n");
    depth++;
    emit_stmt(n);
    depth--;
    indent();
    put("}");
}

static void emit_items(const NodeList *items)
{
    for (int i = 0; i < items->len; i++)
        emit_stmt(items->items[i]);
}

static void emit_if(const Node *n)
{
    put("if (");
    emit_cond(n->u.branch.cond, PREC_ASSIGN);
    put(") ");
    emit_body(n->u.branch.then);
    if (n->u.branch.els) {
        put(" else ");
        if (n->u.branch.els->kind == N_IF)
            emit_if(n->u.branch.els);
        else
            emit_body(n->u.branch.els);
    }
}

static void emit_stmt_inline(const Node *n)
{
    switch (n->kind) {
    case N_BLOCK:
        put("{\n");
        depth++;
        emit_items(n->u.list.items);
        depth--;
        indent();
        put("}");
        break;
    case N_IF:
        emit_if(n);
        break;
    case N_WHILE:
        put("while (");
        emit_cond(n->u.loop.cond, PREC_ASSIGN);
        put(") ");
        emit_body(n->u.loop.body);
        break;
    case N_DO_WHILE:
        put("do ");
        emit_body(n->u.loop.body);
        put(" while (");
        emit_cond(n->u.loop.cond, PREC_ASSIGN);
        put(");");
        break;
    case N_FOR: {
        const Node *init = n->u.loop.init;
        put("for (");
        if (init && init->kind == N_DECL) {
            for (int i = 0; i < init->u.decl.vars->len; i++) {
                const Node *v = init->u.decl.vars->items[i];
                if (i) {
                    put(", %s = ", js_name(v->u.var.name));
                    emit_var_value(v);
                } else {
                    emit_var(v);
                }
            }
        } else if (init) {
            emit_expr(init->u.wrap.expr, PREC_ASSIGN);
        }
        put("; ");
        if (n->u.loop.cond)
            emit_cond(n->u.loop.cond, PREC_ASSIGN);
        put("; ");
        if (n->u.loop.step)
            emit_expr(n->u.loop.step, PREC_ASSIGN);
        put(") ");
        emit_body(n->u.loop.body);
        break;
    }
    default:
        break;
    }
}

static void emit_stmt(const Node *n)
{
    switch (n->kind) {
    case N_DECL:
        for (int i = 0; i < n->u.decl.vars->len; i++) {
            indent();
            emit_var(n->u.decl.vars->items[i]);
            put(";\n");
        }
        break;
    case N_BLOCK:
    case N_IF:
    case N_WHILE:
    case N_DO_WHILE:
    case N_FOR:
        indent();
        emit_stmt_inline(n);
        put("\n");
        break;
    case N_RETURN:
        indent();
        if (n->u.wrap.expr) {
            put("return ");
            emit_converted(n->u.wrap.expr, cur_ret, PREC_ASSIGN);
            put(";\n");
        } else {
            put("return;\n");
        }
        break;
    case N_BREAK:
        indent();
        put("break;\n");
        break;
    case N_CONTINUE:
        indent();
        put("continue;\n");
        break;
    case N_EXPR_STMT:
        indent();
        emit_expr(n->u.wrap.expr, PREC_ASSIGN);
        put(";\n");
        break;
    case N_EMPTY:
        break;
    default:
        break;
    }
}

static void emit_func(const Node *fn)
{
    NodeList *params = fn->u.func.params;
    put("\nfunction %s(", js_name(fn->u.func.name));
    for (int i = 0; i < params->len; i++)
        put("%s%s", i ? ", " : "", js_name(params->items[i]->u.var.name));
    put(") {\n");
    depth = 1;
    cur_ret = fn->u.func.ret;
    emit_items(fn->u.func.body->u.list.items);
    depth = 0;
    put("}\n");
}

char *codegen_js(const Node *root)
{
    Buf body = { NULL, 0, 0 };
    Buf head = { NULL, 0, 0 };
    uses_idiv = uses_imod = uses_toint = uses_printf = 0;
    depth = 0;

    out = &body;
    NodeList *items = root->u.list.items;
    for (int i = 0; i < items->len; i++) {
        const Node *n = items->items[i];
        if (n->kind == N_DECL)
            emit_stmt(n);
        else if (n->kind == N_FUNC && n->u.func.body)
            emit_func(n);
    }
    put("\nconst __status = main();\n");
    put("if (typeof process !== \"undefined\" && !globalThis.__cparseWrite) process.exitCode = __status;\n");

    out = &head;
    put("\"use strict\";\n\n");
    put("const __write = globalThis.__cparseWrite || (s => process.stdout.write(s));\n");
    if (uses_toint)
        put("const __toint = x => Math.trunc(x) | 0;\n");
    if (uses_idiv)
        put("function __idiv(a, b) {\n    if (b === 0) throw new RangeError(\"integer division by zero\");\n    return (a / b) | 0;\n}\n");
    if (uses_imod)
        put("function __imod(a, b) {\n    if (b === 0) throw new RangeError(\"integer remainder by zero\");\n    return (a %% b) | 0;\n}\n");
    if (uses_printf)
        for (const char *const *line = runtime_printf; *line; line++)
            put("%s", *line);
    put("%s", body.data ? body.data : "");
    free(body.data);
    return head.data;
}
