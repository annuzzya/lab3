#ifndef DIAG_H
#define DIAG_H

void diag_set_file(const char *name);
void diag_error(int line, const char *fmt, ...);
int diag_count(void);
const char *diag_file(void);

#endif
