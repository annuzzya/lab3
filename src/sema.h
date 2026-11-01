#ifndef SEMA_H
#define SEMA_H

#include "ast.h"

typedef struct {
    int nglobals;
    Node *main_fn;
} Program;

int sema_analyze(Node *root, Program *prog);

#endif
