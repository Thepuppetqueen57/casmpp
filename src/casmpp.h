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

/* ─────────────────────────────────────────────
   VARIABLE STORE (variables.c)
   ───────────────────────────────────────────── */

extern Variable variables[MAX_VARIABLES]; /* flat array holding all declared variables */
extern int      variable_count;           /* how many variables have been declared so far */

Variable *find_variable(const char *name);
void      add_variable(const char *name, VarType type, const char *value);

/* ─────────────────────────────────────────────
   STRING UTILITY (util.c)
   ───────────────────────────────────────────── */

char *trim_whitespace(const char *str);

/* ─────────────────────────────────────────────
   BUILT-IN COMMANDS (commands.c)
   ───────────────────────────────────────────── */

void cmd_input(char *args);
void cmd_output(char *args, int line_num);
void run_command(char *line, int line_num);

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
   IF BLOCKS AND INTERPRETER (interpreter.c)
   ───────────────────────────────────────────── */

void process_if_block(FILE *file, char *condition_line, int *line_num);
void interpret_file(const char *filename);

#endif