#include "casmpp.h"

/* ─────────────────────────────────────────────
   COMPARISONS AND CONDITIONS
   ───────────────────────────────────────────── */

/* Is this token a bare integer literal such as 42 or -7? */
int is_integer_literal(const char *token) {
    if (*token == '\0') return 0;
    char *endptr;
    strtol(token, &endptr, 10);
    return *endptr == '\0';
}

/* Read one side of a comparison, advancing *cursor past it.
   A quoted value is a text literal, anything else is a number or a variable
   name. is_literal_text is set to 1 for a quoted value. Returns 1 on success,
   or 0 after reporting an error. */
int read_operand(char **cursor, char *out, int *is_literal_text, int line_num) {
    char *p = *cursor;
    while (*p == ' ' || *p == '\t') p++;

    *is_literal_text = 0;
    out[0] = '\0';

    /* A quoted value: keep everything between the quotes, spaces included. */
    if (*p == '"') {
        p++;
        int i = 0;
        while (*p != '\0' && *p != '"') {
            if (i < MAX_STR_LEN - 1) out[i++] = *p;
            p++;
        }
        if (*p != '"') {
            printf(COLOR_RED "Syntax error: unterminated string in condition on line %d" COLOR_RESET "\n", line_num);
            return 0;
        }
        out[i] = '\0';
        *is_literal_text = 1;
        *cursor = p + 1;
        return 1;
    }

    /* A bare value: stops at whitespace, at the comparison operator, or at a
       trailing colon, so "x==5" parses the same as "x == 5" and the Python
       style "if x == 5:" keeps the colon out of the value. */
    int i = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && strchr("=!<>:", *p) == NULL) {
        if (i < MAX_STR_LEN - 1) out[i++] = *p;
        p++;
    }
    out[i] = '\0';

    if (i == 0) {
        printf(COLOR_RED "Syntax error: condition expects: <value> <operator> <value> on line %d (operators: ==, !=, >, <, >=, <=)" COLOR_RESET "\n", line_num);
        return 0;
    }

    *cursor = p;
    return 1;
}

/* Read the comparison operator at *cursor, advancing past it.
   Supported: ==, !=, >, <, >=, <=. Returns 1 on success, or 0 after reporting
   an error describing whatever the user actually typed. */
int read_operator(char **cursor, char *out, int line_num) {
    char *p = *cursor;
    while (*p == ' ' || *p == '\t') p++;

    const char *found = NULL;
    if      (p[0] == '=' && p[1] == '=') found = "==";
    else if (p[0] == '!' && p[1] == '=') found = "!=";
    else if (p[0] == '>' && p[1] == '=') found = ">=";
    else if (p[0] == '<' && p[1] == '=') found = "<=";
    else if (p[0] == '>')                found = ">";
    else if (p[0] == '<')                found = "<";

    /* Nothing at all here means the operator is missing. */
    if (*p == '\0') {
        printf(COLOR_RED "Syntax error: condition expects: <value> <operator> <value> on line %d (operators: ==, !=, >, <, >=, <=)" COLOR_RESET "\n", line_num);
        return 0;
    }

    /* What the user actually wrote, for the error message: the whole run of
       operator characters — so "<>" and "===" are reported in one piece — or,
       when there is none, the next word. */
    char shown[16];
    int  shown_len = 0;
    while (p[shown_len] != '\0' && strchr("=!<>", p[shown_len]) != NULL && shown_len < 15) {
        shown[shown_len] = p[shown_len];
        shown_len++;
    }
    if (shown_len == 0) {
        while (p[shown_len] != '\0' && p[shown_len] != ' ' && p[shown_len] != '\t' && shown_len < 15) {
            shown[shown_len] = p[shown_len];
            shown_len++;
        }
    }
    shown[shown_len] = '\0';

    /* A valid operator must cover every one of those characters. */
    if (!found || (int)strlen(found) != shown_len) {
        printf(COLOR_RED "Error: unknown operator '%s' on line %d (expected ==, !=, >, <, >= or <=)" COLOR_RESET "\n", shown, line_num);
        return 0;
    }

    strcpy(out, found);
    *cursor = p + strlen(found);
    return 1;
}

/* Turn one side of a comparison into an Operand.
     "text"        → the text itself (when is_literal_text is 1)
     42            → the number 42
     somevar       → that variable's current value
   Returns 1 on success, or 0 after reporting an error. */
int resolve_operand(const char *token, int is_literal_text, int line_num, Operand *out) {
    out->int_value = 0;
    out->text[0]   = '\0';

    if (is_literal_text) {
        out->is_text = 1;
        snprintf(out->text, sizeof(out->text), "%s", token);
        return 1;
    }

    if (is_integer_literal(token)) {
        out->is_text   = 0;
        out->int_value = atoi(token);
        return 1;
    }

    Variable *var = find_variable(token);
    if (!var) {
        printf(COLOR_RED "Error: variable '%s' not found (line %d)" COLOR_RESET "\n", token, line_num);
        return 0;
    }

    if (var->type == TYPE_STR) {
        out->is_text = 1;
        snprintf(out->text, sizeof(out->text), "%s", var->value.strValue);
    } else if (var->type == TYPE_SML) {
        out->is_text   = 0;
        out->int_value = var->value.smlValue;
    } else {
        out->is_text   = 0;
        out->int_value = var->value.intValue;
    }
    return 1;
}

/* Compare two operands, returning a negative number, 0, or a positive number.
   Two numbers compare numerically; if either side is text both sides are
   compared as text, so 5 and "5" count as equal. */
int compare_operands(const Operand *a, const Operand *b) {
    if (!a->is_text && !b->is_text) {
        return (a->int_value > b->int_value) - (a->int_value < b->int_value);
    }

    char a_text[MAX_STR_LEN];
    char b_text[MAX_STR_LEN];
    if (a->is_text) snprintf(a_text, sizeof(a_text), "%s", a->text);
    else            snprintf(a_text, sizeof(a_text), "%d", a->int_value);
    if (b->is_text) snprintf(b_text, sizeof(b_text), "%s", b->text);
    else            snprintf(b_text, sizeof(b_text), "%d", b->int_value);

    return strcmp(a_text, b_text);
}

/* Evaluate a branch header — "if <value> <operator> <value>" or the same with
   "elif" — and store its truth value in *out_truth. Either side may be a
   variable, a number or a quoted string, so all of these work:
       if x == 5        if x != y        if 5 > 3        if name == "bob"
   Returns 1 when the header is valid, or 0 after reporting an error. */
int evaluate_condition(const char *header, int line_num, int *out_truth) {
    char buf[MAX_STR_LEN];
    snprintf(buf, sizeof(buf), "%s", header);

    /* Skip the "if" / "elif" keyword. */
    char *p = buf;
    while (*p != '\0' && *p != ' ' && *p != '\t') p++;

    char lhs_token[MAX_STR_LEN];
    char rhs_token[MAX_STR_LEN];
    char op[16];
    int  lhs_is_text, rhs_is_text;

    if (!read_operand(&p, lhs_token, &lhs_is_text, line_num)) return 0;
    if (!read_operator(&p, op, line_num))                     return 0;
    if (!read_operand(&p, rhs_token, &rhs_is_text, line_num)) return 0;

    /* Only an optional trailing colon may follow the right-hand side, so the
       Python style "if x == 5:" is accepted as well as "if x == 5". */
    while (*p == ' ' || *p == '\t') p++;
    if (*p == ':') {
        p++;
        while (*p == ' ' || *p == '\t') p++;
    }
    if (*p != '\0') {
        printf(COLOR_RED "Syntax error: unexpected text '%s' after condition on line %d" COLOR_RESET "\n", p, line_num);
        return 0;
    }

    Operand lhs;
    Operand rhs;
    if (!resolve_operand(lhs_token, lhs_is_text, line_num, &lhs)) return 0;
    if (!resolve_operand(rhs_token, rhs_is_text, line_num, &rhs)) return 0;

    int cmp = compare_operands(&lhs, &rhs);

    if      (strcmp(op, "==") == 0) *out_truth = (cmp == 0);
    else if (strcmp(op, "!=") == 0) *out_truth = (cmp != 0);
    else if (strcmp(op, ">")  == 0) *out_truth = (cmp > 0);
    else if (strcmp(op, "<")  == 0) *out_truth = (cmp < 0);
    else if (strcmp(op, ">=") == 0) *out_truth = (cmp >= 0);
    else                            *out_truth = (cmp <= 0); /* <= */

    return 1;
}

/* Does this line consist of the given keyword, optionally followed by a colon?
   This lets the Python style "else:" work alongside plain "else". */
int is_keyword_line(const char *line, const char *keyword) {
    size_t len = strlen(keyword);
    if (strncmp(line, keyword, len) != 0) return 0;
    if (line[len] == '\0')             return 1;
    return line[len] == ':' && line[len + 1] == '\0';
}