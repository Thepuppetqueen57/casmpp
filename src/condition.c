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

/* Copy the text between start and end into out, without the spaces around it
   or the optional Python style colon at the very end. */
static void copy_part(const char *start, const char *end, char *out, size_t out_size) {
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == ':')) end--;

    size_t length = (size_t)(end - start);
    if (length >= out_size) length = out_size - 1;

    memcpy(out, start, length);
    out[length] = '\0';
}

/* Evaluate a branch header — "if <value> <operator> <value>" or the same with
   "elif" — and store its truth value in *out_truth. Either side may be an
   expression, so all of these work:
       if x == 5        if x + 1 > y * 2     if name == "bob"
   Returns 1 when the header is valid, or 0 after reporting an error. */
int evaluate_condition(const char *header, int line_num, int *out_truth) {
    const char *condition = header;

    /* Skip the "if" / "elif" / "while" keyword. */
    while (*condition != ' ' && *condition != '\t' && *condition != '\0') condition++;

    /* The comparison sits outside any parentheses, so the ones inside an
       expression are left to the expression itself. */
    const char *op_at  = NULL;
    const char *op     = NULL;
    int         depth  = 0;

    for (const char *p = condition; *p != '\0'; p++) {
        if (*p == '(') {
            depth++;
            continue;
        }
        if (*p == ')') {
            if (depth > 0) depth--;
            continue;
        }
        if (depth != 0) continue;
        if (*p != '=' && *p != '!' && *p != '<' && *p != '>') continue;

        /* The whole run of operator characters, so "<>" and "===" are reported
           in one piece instead of being read as something shorter. */
        char shown[16];
        int  shown_len = 0;
        for (const char *r = p; *r != '\0' && strchr("=!<>", *r) != NULL && shown_len < 15; r++) {
            shown[shown_len++] = *r;
        }
        shown[shown_len] = '\0';

        if      (strcmp(shown, "==") == 0) op = "==";
        else if (strcmp(shown, "!=") == 0) op = "!=";
        else if (strcmp(shown, ">=") == 0) op = ">=";
        else if (strcmp(shown, "<=") == 0) op = "<=";
        else if (strcmp(shown, ">")  == 0) op = ">";
        else if (strcmp(shown, "<")  == 0) op = "<";

        if (!op) {
            printf(COLOR_RED "Error: unknown operator '%s' on line %d (expected ==, !=, >, <, >= or <=)" COLOR_RESET "\n", shown, line_num);
            return 0;
        }

        op_at = p;
        break;
    }

    if (!op_at) {
        printf(COLOR_RED "Syntax error: condition expects: <value> <operator> <value> on line %d (operators: ==, !=, >, <, >=, <=)" COLOR_RESET "\n", line_num);
        return 0;
    }

    char lhs_text[MAX_STR_LEN];
    char rhs_text[MAX_STR_LEN];

    copy_part(condition, op_at, lhs_text, sizeof(lhs_text));
    copy_part(op_at + strlen(op), condition + strlen(condition), rhs_text, sizeof(rhs_text));

    Operand lhs;
    Operand rhs;
    if (!eval_expression(lhs_text, line_num, &lhs)) return 0;
    if (!eval_expression(rhs_text, line_num, &rhs)) return 0;

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