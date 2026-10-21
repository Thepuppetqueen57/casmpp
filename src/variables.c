#include "casmpp.h"

/* ─────────────────────────────────────────────
   GLOBAL VARIABLE STORE
   ───────────────────────────────────────────── */

Variable variables[MAX_VARIABLES]; /* flat array holding all declared variables */
int      variable_count = 0;       /* how many variables have been declared so far */

/* ─────────────────────────────────────────────
   VARIABLE HELPERS
   ───────────────────────────────────────────── */

/* Look up a variable by name.
   Returns a pointer to the Variable if found, or NULL if it doesn't exist. */
Variable *find_variable(const char *name) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return &variables[i];
        }
    }
    return NULL;
}

/* Declare a variable, or redefine it when the name is already taken.
   - name  : the variable's identifier
   - type  : TYPE_INT, TYPE_STR, or TYPE_SML
   - value : the value as a raw string (e.g. "42", "hello", "1")

   Redefining updates the existing entry in place. Appending a second entry
   would leave the old value in front of it, where every lookup would keep
   finding the old one. */
void add_variable(const char *name, VarType type, const char *value) {
    Variable *var = find_variable(name);

    if (!var) {
        if (variable_count >= MAX_VARIABLES) {
            printf(COLOR_RED "Error: Too many variables (limit is %d)" COLOR_RESET "\n", MAX_VARIABLES);
            return;
        }
        var = &variables[variable_count++];
        snprintf(var->name, sizeof(var->name), "%s", name);
    }

    var->type = type;

    if (type == TYPE_INT) {
        var->value.intValue = atoi(value);
    } else if (type == TYPE_STR) {
        snprintf(var->value.strValue, sizeof(var->value.strValue), "%s", value);
    } else if (type == TYPE_SML) {
        /* Only "1" counts as true; everything else is 0. */
        var->value.smlValue = (strcmp(value, "1") == 0) ? 1 : 0;
    }
}

/* Store a checked literal into a variable.
   Used by declarations, so a bad value is reported instead of quietly turning
   into 0 — "int x = x + 1" is a mistake worth pointing at, not a variable that
   suddenly holds 0. Returns 1 on success, or 0 after reporting an error. */
int set_variable_literal(Variable *var, VarType type, const char *value, int line_num) {
    if (type == TYPE_INT) {
        char *endptr;
        errno = 0;
        long number = strtol(value, &endptr, 10);

        if (value[0] == '\0' || *endptr != '\0' || errno == ERANGE) {
            printf(COLOR_RED "Error: '%s' is not a valid integer value for variable '%s' on line %d (use '%s += 1' for math)" COLOR_RESET "\n",
                   value, var->name, line_num, var->name);
            return 0;
        }

        var->type           = TYPE_INT;
        var->value.intValue = (int)number;
        return 1;
    }

    if (type == TYPE_STR) {
        var->type = TYPE_STR;
        snprintf(var->value.strValue, sizeof(var->value.strValue), "%s", value);
        return 1;
    }

    if (strcmp(value, "0") != 0 && strcmp(value, "1") != 0) {
        printf(COLOR_RED "Error: sml variable '%s' must be 0 or 1, got '%s' on line %d" COLOR_RESET "\n", var->name, value, line_num);
        return 0;
    }

    var->type           = TYPE_SML;
    var->value.smlValue = value[0] == '1' ? 1 : 0;
    return 1;
}

/* Apply a math assignment — +=, -=, *= or /= — to an int variable.
   The right-hand side may be a number or another numeric variable, so both
   "int score += 1" and "int score += bonus" work. Math is only defined for
   int variables. Returns 1 on success, or 0 after reporting an error. */
int set_variable_math(Variable *var, const char *op, const char *rhs, int line_num) {
    if (var->type != TYPE_INT) {
        printf(COLOR_RED "Error: variable '%s' is not an int, so '%s' cannot be used on it (line %d)" COLOR_RESET "\n",
               var->name, op, line_num);
        return 0;
    }

    int amount;
    if (is_integer_literal(rhs)) {
        amount = atoi(rhs);
    } else {
        Variable *other = find_variable(rhs);
        if (!other) {
            printf(COLOR_RED "Error: variable '%s' not found (line %d)" COLOR_RESET "\n", rhs, line_num);
            return 0;
        }
        if (other->type == TYPE_STR) {
            printf(COLOR_RED "Error: variable '%s' is not a number, so it cannot be used with '%s' (line %d)" COLOR_RESET "\n",
                   other->name, op, line_num);
            return 0;
        }
        amount = other->type == TYPE_SML ? other->value.smlValue : other->value.intValue;
    }

    if (strcmp(op, "+=") == 0) {
        var->value.intValue += amount;
    } else if (strcmp(op, "-=") == 0) {
        var->value.intValue -= amount;
    } else if (strcmp(op, "*=") == 0) {
        var->value.intValue *= amount;
    } else {
        if (amount == 0) {
            printf(COLOR_RED "Error: division by zero in '%s /= 0' on line %d" COLOR_RESET "\n", var->name, line_num);
            return 0;
        }
        var->value.intValue /= amount;
    }

    return 1;
}