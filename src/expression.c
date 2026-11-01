#include "casmpp.h"

/* ─────────────────────────────────────────────
   EXPRESSIONS
   ───────────────────────────────────────────── */

/* One parse in progress: where we are in the text, the line number for error
   messages, and whether a mistake has already been reported. Once failed is
   set the rest of the parse unwinds without printing anything more. */
typedef struct {
    const char *p;
    int         line_num;
    int         failed;
} Expr;

static int parse_sum(Expr *e, Operand *out);

static void skip_spaces(Expr *e) {
    while (*e->p == ' ' || *e->p == '\t') e->p++;
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* Both sides of + - * / % have to be numbers, so text is reported instead of
   quietly turning into 0. */
static int require_numbers(Expr *e, const Operand *left, const Operand *right, char op) {
    if (!left->is_text && !right->is_text) return 1;

    printf(COLOR_RED "Error: only numbers can be used with '%c' on line %d" COLOR_RESET "\n",
           op, e->line_num);
    e->failed = 1;
    return 0;
}

/* A value: a parenthesised expression, a quoted string, a call, or a number or
   variable name. */
static int parse_primary(Expr *e, Operand *out) {
    skip_spaces(e);

    out->is_text   = 0;
    out->int_value = 0;
    out->text[0]   = '\0';

    if (*e->p == '(') {
        e->p++;
        if (!parse_sum(e, out)) return 0;

        skip_spaces(e);
        if (*e->p != ')') {
            printf(COLOR_RED "Syntax error: missing ')' in the expression on line %d" COLOR_RESET "\n", e->line_num);
            e->failed = 1;
            return 0;
        }
        e->p++;
        return 1;
    }

    if (*e->p == '"') {
        e->p++;
        int i = 0;
        while (*e->p != '\0' && *e->p != '"') {
            if (i < MAX_STR_LEN - 1) out->text[i++] = *e->p;
            e->p++;
        }
        if (*e->p != '"') {
            printf(COLOR_RED "Syntax error: unterminated string in the expression on line %d" COLOR_RESET "\n", e->line_num);
            e->failed = 1;
            return 0;
        }
        out->text[i] = '\0';
        out->is_text = 1;
        e->p++;
        return 1;
    }

    if (isdigit((unsigned char)*e->p)) {
        char number[32];
        int  i = 0;

        while (isdigit((unsigned char)*e->p)) {
            if (i < (int)sizeof(number) - 1) number[i++] = *e->p;
            e->p++;
        }
        number[i] = '\0';

        out->is_text   = 0;
        out->int_value = atoi(number);
        return 1;
    }

    if (is_ident_start(*e->p)) {
        char name[MAX_NAME_LEN];
        int  i = 0;
        while (is_ident_char(*e->p)) {
            if (i < MAX_NAME_LEN - 1) name[i++] = *e->p;
            e->p++;
        }
        name[i] = '\0';

        /* A name followed by "(" is a call to a function, and what it returns
           is the value. The whole balanced call is handed over as one piece,
           because its arguments are parsed on their own. */
        const char *after_name = e->p;
        skip_spaces(e);

        if (*e->p == '(') {
            const char *open  = e->p;
            int         depth = 0;

            while (*e->p != '\0') {
                if (*e->p == '(') {
                    depth++;
                } else if (*e->p == ')') {
                    depth--;
                    if (depth == 0) {
                        e->p++;
                        break;
                    }
                }
                e->p++;
            }

            if (depth != 0) {
                printf(COLOR_RED "Syntax error: missing ')' in the call to '%s' on line %d" COLOR_RESET "\n",
                       name, e->line_num);
                e->failed = 1;
                return 0;
            }

            char   call[MAX_STR_LEN * 2];
            size_t name_length = strlen(name);
            size_t args_length = (size_t)(e->p - open);

            if (name_length + args_length >= sizeof(call)) {
                args_length = sizeof(call) - name_length - 1;
            }

            memcpy(call, name, name_length);
            memcpy(call + name_length, open, args_length);
            call[name_length + args_length] = '\0';

            if (!call_for_value(call, e->line_num, out)) {
                e->failed = 1;
                return 0;
            }
            return 1;
        }

        e->p = after_name;
        return resolve_operand(name, 0, e->line_num, out);
    }

    printf(COLOR_RED "Syntax error: expected a value in the expression on line %d" COLOR_RESET "\n", e->line_num);
    e->failed = 1;
    return 0;
}

/* A value with an optional sign in front of it. */
static int parse_unary(Expr *e, Operand *out) {
    skip_spaces(e);

    if (*e->p == '+') {
        e->p++;
        return parse_unary(e, out);
    }

    if (*e->p == '-') {
        e->p++;
        if (!parse_unary(e, out)) return 0;

        if (out->is_text) {
            printf(COLOR_RED "Error: '-' cannot be used on text on line %d" COLOR_RESET "\n", e->line_num);
            e->failed = 1;
            return 0;
        }

        out->int_value = -out->int_value;
        return 1;
    }

    return parse_primary(e, out);
}

/* * / and %, which bind tighter than + and -. */
static int parse_product(Expr *e, Operand *out) {
    if (!parse_unary(e, out)) return 0;

    for (;;) {
        skip_spaces(e);

        char op = *e->p;
        if (op != '*' && op != '/' && op != '%') return 1;
        e->p++;

        Operand right;
        if (!parse_unary(e, &right)) return 0;
        if (!require_numbers(e, out, &right, op)) return 0;

        if ((op == '/' || op == '%') && right.int_value == 0) {
            printf(COLOR_RED "Error: division by zero on line %d" COLOR_RESET "\n", e->line_num);
            e->failed = 1;
            return 0;
        }

        if (op == '*')      out->int_value *= right.int_value;
        else if (op == '/') out->int_value /= right.int_value;
        else                out->int_value %= right.int_value;
    }
}

/* + and -, which bind loosest. */
static int parse_sum(Expr *e, Operand *out) {
    if (!parse_product(e, out)) return 0;

    for (;;) {
        skip_spaces(e);

        char op = *e->p;
        if (op != '+' && op != '-') return 1;
        e->p++;

        Operand right;
        if (!parse_product(e, &right)) return 0;
        if (!require_numbers(e, out, &right, op)) return 0;

        if (op == '+') out->int_value += right.int_value;
        else           out->int_value -= right.int_value;
    }
}

/* Work out the value of an expression: numbers, variables, calls, parentheses
   and the operators + - * / % with unary signs. The result is a number, or the
   text of a quoted string or of a function that returned text. Returns 1 on
   success, or 0 after reporting an error. */
int eval_expression(const char *text, int line_num, Operand *out) {
    Expr e;
    e.p        = text;
    e.line_num = line_num;
    e.failed   = 0;

    skip_spaces(&e);
    if (*e.p == '\0') {
        printf(COLOR_RED "Syntax error: expected a value on line %d" COLOR_RESET "\n", line_num);
        return 0;
    }

    if (!parse_sum(&e, out)) return 0;

    skip_spaces(&e);
    if (*e.p != '\0') {
        printf(COLOR_RED "Syntax error: unexpected text '%s' in the expression on line %d" COLOR_RESET "\n",
               e.p, line_num);
        return 0;
    }

    return 1;
}

/* Like eval_expression, for the places that can only hold a number. */
int eval_int(const char *text, int line_num, int *out_value) {
    Operand result;

    if (!eval_expression(text, line_num, &result)) return 0;

    if (result.is_text) {
        printf(COLOR_RED "Error: '%s' is text, but a number is needed here on line %d" COLOR_RESET "\n",
               text, line_num);
        return 0;
    }

    *out_value = result.int_value;
    return 1;
}