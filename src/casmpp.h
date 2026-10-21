#ifndef CASMPP_H
#define CASMPP_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

/* ─────────────────────────────────────────────
   TERMINAL COLORS
   ───────────────────────────────────────────── */

/* ANSI escape sequences used to color diagnostic output.
   Warnings are printed in yellow, errors in red. */
#define COLOR_RED    "\x1b[31m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_RESET  "\x1b[0m"

/* ─────────────────────────────────────────────
   CONSTANTS
   ───────────────────────────────────────────── */

#define MAX_VARIABLES 100   /* Maximum number of variables a script can declare */
#define MAX_NAME_LEN  50    /* Maximum length of a variable name */
#define MAX_STR_LEN   256   /* Maximum length of a string variable's value */
#define MAX_RUN_DEPTH 32    /* How many 'run' files may be open at the same time */
#define MAX_BLOCK_DEPTH 64  /* How deeply if/while/for blocks may nest */

/* ─────────────────────────────────────────────
   TYPES
   ───────────────────────────────────────────── */

/* The three variable types supported by CASM++:
     int  — a whole number (e.g. 42)
     str  — a piece of text (e.g. "hello")
     sml  — a tiny flag, either 0 or 1 */
typedef enum {
    TYPE_INT,
    TYPE_STR,
    TYPE_SML
} VarType;

/* One variable: a name, a type, and a value. */
typedef struct {
    char   name[MAX_NAME_LEN];
    VarType type;
    union {
        int  intValue;          /* used when type == TYPE_INT */
        char strValue[MAX_STR_LEN]; /* used when type == TYPE_STR */
        int  smlValue;          /* used when type == TYPE_SML (0 or 1) */
    } value;
} Variable;

/* One side of a comparison. A side is either a whole number or a piece of
   text; which one it is decides how the two sides are compared. */
typedef struct {
    int  is_text;
    int  int_value;
    char text[MAX_STR_LEN];
} Operand;

/* How a block finished. A loop body that runs "break" reports FLOW_BREAK so
   the enclosing loop stops; every other path reports FLOW_NORMAL. */
typedef enum {
    FLOW_NORMAL,
    FLOW_BREAK
} Flow;

/* One executable line of a script: its text (trimmed, without a trailing
   semicolon), the line number it came from, and — once worked out — the index
   of the "end" that closes it, when the line opens a block. */
typedef struct {
    char *text;
    int   line_num;
    int   match;      /* 0 = not worked out yet, -1 = no matching "end" */
} ScriptLine;

/* A whole script held in memory, so blocks and loops can jump around freely
   instead of being read from a file one line at a time. */
typedef struct {
    ScriptLine *lines;
    int         count;         /* executable lines (blank and # lines dropped) */
    int         capacity;
    int         last_line_num; /* lines read from the file, for error messages */
} Script;

/* ─────────────────────────────────────────────
   VARIABLE STORE (variables.c)
   ───────────────────────────────────────────── */

extern Variable variables[MAX_VARIABLES]; /* flat array holding all declared variables */
extern int      variable_count;           /* how many variables have been declared so far */

Variable *find_variable(const char *name);
void      add_variable(const char *name, VarType type, const char *value);
int       set_variable_literal(Variable *var, VarType type, const char *value, int line_num);
int       set_variable_math(Variable *var, const char *op, const char *rhs, int line_num);

/* ─────────────────────────────────────────────
   STRING UTILITY (util.c)
   ───────────────────────────────────────────── */

char *trim_whitespace(const char *str);
void  path_directory(const char *path, char *out, size_t out_size);
void  join_path(const char *dir, const char *relative, char *out, size_t out_size);

/* ─────────────────────────────────────────────
   BUILT-IN COMMANDS (commands.c)
   ───────────────────────────────────────────── */

void cmd_input(char *args);
void cmd_output(char *args, int line_num);
void run_command(const char *line, int line_num);

/* ─────────────────────────────────────────────
   CONDITIONS (condition.c)
   ───────────────────────────────────────────── */

int is_integer_literal(const char *token);
int read_operand(char **cursor, char *out, int *is_literal_text, int line_num);
int read_operator(char **cursor, char *out, int line_num);
int resolve_operand(const char *token, int is_literal_text, int line_num, Operand *out);
int compare_operands(const Operand *a, const Operand *b);
int evaluate_condition(const char *header, int line_num, int *out_truth);
int is_keyword_line(const char *line, const char *keyword);

/* ─────────────────────────────────────────────
   SCRIPT STORAGE AND BLOCK HEADERS (script.c)
   ───────────────────────────────────────────── */

int  script_load(const char *filename, Script *script);
int  script_find_end(Script *script, int opener);
void script_free(Script *script);

int is_if_header(const char *line);
int is_elif_header(const char *line);
int is_else_header(const char *line);
int is_while_header(const char *line);
int is_for_header(const char *line);
int is_end_line(const char *line);
int is_block_opener(const char *line);

/* ─────────────────────────────────────────────
   DECLARATIONS AND ASSIGNMENTS (declaration.c)
   ───────────────────────────────────────────── */

int is_declaration_line(const char *line);
int exec_declaration(const char *line, int line_num);

/* ─────────────────────────────────────────────
   IF BLOCKS AND INTERPRETER (interpreter.c)
   ───────────────────────────────────────────── */

void interpret_file(const char *filename);
void cmd_run(char *args, int line_num); /* runs another script in place of the run line */

#endif