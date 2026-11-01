CC      ?= cc
BISON   ?= bison
FLEX    ?= flex
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L
GENFLAGS = -std=c99 -O2 -D_POSIX_C_SOURCE=200809L -Wno-unused-function -Wno-sign-compare -Wno-unneeded-internal-declaration
LDLIBS  := -lm
BFLAGS  := $(shell $(BISON) -Wno-deprecated --version >/dev/null 2>&1 && echo -Wno-deprecated)

BUILD   := build
SRC     := src
OBJS    := $(BUILD)/ast.o $(BUILD)/diag.o $(BUILD)/format.o $(BUILD)/sema.o \
           $(BUILD)/interp.o $(BUILD)/codegen.o $(BUILD)/main.o \
           $(BUILD)/parser.tab.o $(BUILD)/lex.yy.o

EXAMPLE ?= examples/sort.c

all: cparse

cparse: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h: $(SRC)/parser.y | $(BUILD)
	$(BISON) $(BFLAGS) -d -v -o $(BUILD)/parser.tab.c $<

$(BUILD)/lex.yy.c: $(SRC)/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	$(FLEX) -o $@ $<

$(BUILD)/parser.tab.o: $(BUILD)/parser.tab.c $(SRC)/ast.h $(SRC)/diag.h
	$(CC) $(GENFLAGS) -I$(SRC) -c $< -o $@

$(BUILD)/lex.yy.o: $(BUILD)/lex.yy.c $(SRC)/ast.h $(SRC)/diag.h
	$(CC) $(GENFLAGS) -I$(SRC) -I$(BUILD) -c $< -o $@

$(BUILD)/main.o: $(SRC)/main.c $(BUILD)/parser.tab.h
	$(CC) $(CFLAGS) -I$(BUILD) -c $< -o $@

$(BUILD)/%.o: $(SRC)/%.c $(SRC)/ast.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

test: cparse
	@sh tests/run.sh

web: cparse
	./cparse --web web $(EXAMPLE)

conflicts: | $(BUILD)
	@for g in grammars/*.y; do \
		echo "== $$g"; \
		$(BISON) -v -o $(BUILD)/$$(basename $$g .y).tab.c $$g 2>&1 | sed 's/^/   /'; \
		grep -E '^State [0-9]+ conflicts' $(BUILD)/$$(basename $$g .y).output | sed 's/^/   /'; \
	done

clean:
	rm -rf $(BUILD) cparse

.PHONY: all test web conflicts clean
