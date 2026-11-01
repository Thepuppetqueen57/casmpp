#include "casmpp.h"

/* ─────────────────────────────────────────────
   VARIABLE DECLARATIONS AND ASSIGNMENTS
   ───────────────────────────────────────────── */

/* Does this line start with one of the three type keywords?
   Format:  <type> <name> = <value>      declare or redefine
        or:  <type> <name> <op> <value>  math on an existing int
   Example: int score = 0
            str greeting = "hello"
            sml flag = 1
            int score += 1                                                    */
int is_declaration_line(const char *line) {
    return strncmp(line, "int ", 4) == 0 ||
           strncmp(line, "str ", 4) == 0 ||
           strncmp(line, "sml ", 4) == 0;
}

/* Which type does the line start with? Only called for lines that passed
   is_declaration_line(). */
static VarType type_from_keyword(const char *line) {
    if (strncmp(line, "int", 3) == 0) return TYPE_INT;
    if (strncmp(line, "str", 3) == 0) return TYPE_STR;
    return TYPE_SML;
}

/* Parse "<type> <name> <operator> <value>" into its three parts.
   Returns 1 on success, or 0 after reporting a syntax error. */
static int parse_declaration(const char *line, VarType type, char *name,
                             char *op, char *value, int line_num) {
    const char *p = line + 3; /* past "int" / "str" / "sml" */
    while (*p == ' ' || *p == '\t') p++;

    int i = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '=' &&
           *p != '+' && *p != '-' && *p != '*' && *p != '/') {
        if (i < MAX_NAME_LEN - 1) name[i++] = *p;
        p++;
    }
    name[i] = '\0';

    while (*p == ' ' || *p == '\t') p++;

    if (name[0] == '\0') {
        printf(COLOR_RED "Syntax error: expected a variable name on line %d (example: int x = 5)" COLOR_RESET "\n", line_num);
        return 0;
    }

    /* The operator is "=" or one of += -= *= /=. Anything else — including a
       bare "int x" — is reported instead of silently declaring nothing. */
    if ((p[0] == '+' || p[0] == '-' || p[0] == '*' || p[0] == '/') && p[1] == '=') {
        op[0] = p[0];
        op[1] = '=';
        op[2] = '\0';
        p += 2;
    } else if (p[0] == '=' && p[1] != '=') {
        snprintf(op, 4, "=");
        p++;
    } else {
        printf(COLOR_RED "Syntax error: expected '=' or one of += -= *= /= after '%s' on line %d" COLOR_RESET "\n", name, line_num);
        return 0;
    }

    while (*p == ' ' || *p == '\t') p++;

    if (*p == '\0') {
        printf(COLOR_RED "Syntax error: '%s' needs a value on line %d (example: %s %s = 0)" COLOR_RESET "\n",
               name, line_num, type == TYPE_INT ? "int" : (type == TYPE_STR ? "str" : "sml"), name);
        return 0;
    }

    /* Text values may be quoted so they can contain spaces, and "" is a valid
       empty string. Anything unquoted is taken as written, closing quote and
       all, which is what the old parser did too. */
    if (type == TYPE_STR && p[0] == '"') {
        const char *closing = strchr(p + 1, '"');
        if (closing) {
            size_t length = (size_t)(closing - (p + 1));
            if (length > MAX_STR_LEN - 1) length = MAX_STR_LEN - 1;
            memcpy(value, p + 1, length);
            value[length] = '\0';
        } else {
            snprintf(value, MAX_STR_LEN, "%s", p + 1);
        }
    } else {
        snprintf(value, MAX_STR_LEN, "%s", p);
    }

    return 1;
}

/* Run one declaration or assignment line.
   Returns 1 when the line was a declaration (whether or not it succeeded) and
   0 when it was not one at all. */
int exec_declaration(const char *line, int line_num) {
    VarType type = type_from_keyword(line);
    char    name[MAX_NAME_LEN];
    char    op[4];
    char    value[MAX_STR_LEN];

    if (!parse_declaration(line, type, name, op, value, line_num)) return 1;

    /* "=" may only touch the variables of the call that is running, so a
       declaration inside a function shadows a top-level name instead of
       overwriting it. Math changes the nearest variable that already exists,
       which lets a function update a top-level one. */
    Variable *existing = strcmp(op, "=") == 0 ? find_variable_local(name) : find_variable(name);

    /* "=" sets the value, creating the variable the first time and redefining
       it afterwards. */
    if (strcmp(op, "=") == 0) {
        Variable prepared;
        snprintf(prepared.name, sizeof(prepared.name), "%s", name);

        if (!set_variable_literal(&prepared, type, value, line_num)) return 1;

        if (existing) {
            existing->type  = prepared.type;
            existing->value = prepared.value;
        } else {
            add_variable(name, type, value);
        }

        return 1;
    }

    /* A math assignment changes a variable that already exists. */
    if (!existing) {
        printf(COLOR_RED "Error: variable '%s' not found, declare it before using '%s' on line %d" COLOR_RESET "\n", name, op, line_num);
        return 1;
    }

    set_variable_math(existing, op, value, line_num);
    return 1;
}