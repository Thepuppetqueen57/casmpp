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

/* Add a new variable to the store.
   - name  : the variable's identifier
   - type  : TYPE_INT, TYPE_STR, or TYPE_SML
   - value : the initial value as a raw string (e.g. "42", "hello", "1") */
void add_variable(const char *name, VarType type, const char *value) {
    if (variable_count >= MAX_VARIABLES) {
        printf(COLOR_RED "Error: Too many variables (limit is %d)" COLOR_RESET "\n", MAX_VARIABLES);
        return;
    }

    Variable *var = &variables[variable_count++];
    strcpy(var->name, name);
    var->type = type;

    if (type == TYPE_INT) {
        var->value.intValue = atoi(value);
    } else if (type == TYPE_STR) {
        strcpy(var->value.strValue, value);
    } else if (type == TYPE_SML) {
        /* Only "1" counts as true; everything else is 0. */
        var->value.smlValue = (strcmp(value, "1") == 0) ? 1 : 0;
    }
}