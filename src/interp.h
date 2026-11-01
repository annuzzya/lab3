#ifndef INTERP_H
#define INTERP_H

#include "ast.h"
#include "sema.h"

int interp_run(Node *root, const Program *prog, int *exit_code);

#endif
