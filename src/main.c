#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "diag.h"
#include "interp.h"
#include "sema.h"

int yyparse(void);
void lexer_reset(FILE *in);
void dump_tokens(FILE *in, FILE *out);

extern Node *parse_result;

typedef struct {
    int tokens;
    int ast;
    int run;
    const char *json;
    const char *js;
    const char *web;
    const char *input;
} Options;

static void usage(FILE *out)
{
    fputs("usage: cparse [options] file.c\n"
          "\n"
          "  --tokens        print the token stream produced by the lexer\n"
          "  --ast           print the annotated syntax tree\n"
          "  --run           interpret the program (default when no other action is given)\n"
          "  --json FILE     write the syntax tree as JSON\n"
          "  --js FILE       translate to JavaScript ('-' writes to stdout)\n"
          "  --web DIR       write DIR/data.js for the browser visualizer\n"
          "  -h, --help      show this help\n",
          out);
}

static int parse_args(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--tokens")) {
            o->tokens = 1;
        } else if (!strcmp(a, "--ast")) {
            o->ast = 1;
        } else if (!strcmp(a, "--run")) {
            o->run = 1;
        } else if (!strcmp(a, "--json") && i + 1 < argc) {
            o->json = argv[++i];
        } else if (!strcmp(a, "--js") && i + 1 < argc) {
            o->js = argv[++i];
        } else if (!strcmp(a, "--web") && i + 1 < argc) {
            o->web = argv[++i];
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage(stdout);
            exit(0);
        } else if (a[0] == '-' && a[1]) {
            fprintf(stderr, "cparse: unknown option '%s'\n", a);
            return 0;
        } else if (!o->input) {
            o->input = a;
        } else {
            fprintf(stderr, "cparse: only one input file is supported\n");
            return 0;
        }
    }
    if (!o->input)
        return 0;
    if (!o->tokens && !o->ast && !o->json && !o->js && !o->web)
        o->run = 1;
    return 1;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *text = malloc((size_t)size + 1);
    size_t got = fread(text, 1, (size_t)size, f);
    text[got] = '\0';
    fclose(f);
    return text;
}

static FILE *open_out(const char *path)
{
    if (!strcmp(path, "-"))
        return stdout;
    FILE *f = fopen(path, "w");
    if (!f)
        perror(path);
    return f;
}

static void close_out(FILE *f)
{
    if (f && f != stdout)
        fclose(f);
}

static int write_web(const char *dir, const char *name, const char *source, const Node *root, const char *js)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/data.js", dir);
    FILE *f = open_out(path);
    if (!f)
        return 0;
    fputs("window.CPARSE_DATA = {\"file\":", f);
    json_string(f, name);
    fputs(",\"source\":", f);
    json_string(f, source);
    fputs(",\"ast\":", f);
    ast_json(f, root);
    fputs(",\"js\":", f);
    if (js)
        json_string(f, js);
    else
        fputs("null", f);
    fputs("};\n", f);
    close_out(f);
    fprintf(stderr, "cparse: wrote %s\n", path);
    return 1;
}

int main(int argc, char **argv)
{
    Options opt;
    if (!parse_args(argc, argv, &opt)) {
        usage(stderr);
        return 2;
    }

    char *source = read_file(opt.input);
    FILE *in = fopen(opt.input, "r");
    if (!source || !in) {
        perror(opt.input);
        return 2;
    }
    diag_set_file(opt.input);

    if (opt.tokens) {
        dump_tokens(in, stdout);
        rewind(in);
    }

    lexer_reset(in);
    int parsed = yyparse() == 0 && diag_count() == 0;
    fclose(in);
    if (!parsed || !parse_result) {
        fprintf(stderr, "cparse: %d error%s, stopping\n", diag_count(), diag_count() == 1 ? "" : "s");
        ast_free(parse_result);
        free(source);
        return 1;
    }

    Program prog;
    int valid = sema_analyze(parse_result, &prog);
    int status = valid ? 0 : 1;

    if (opt.ast)
        ast_print(stdout, parse_result);

    if (opt.json) {
        FILE *f = open_out(opt.json);
        if (f) {
            ast_json(f, parse_result);
            fputc('\n', f);
            close_out(f);
        }
    }

    char *js = valid && (opt.js || opt.web) ? codegen_js(parse_result) : NULL;
    if (js && opt.js) {
        FILE *f = open_out(opt.js);
        if (f) {
            fputs(js, f);
            close_out(f);
        }
    }
    if (opt.web && !write_web(opt.web, opt.input, source, parse_result, js))
        status = 1;

    if (!valid) {
        fprintf(stderr, "cparse: %d error%s\n", diag_count(), diag_count() == 1 ? "" : "s");
    } else if (opt.run) {
        int code = 0;
        if (!interp_run(parse_result, &prog, &code))
            status = 1;
        else
            status = code & 0xff;
    }

    free(js);
    ast_free(parse_result);
    free(source);
    return status;
}
