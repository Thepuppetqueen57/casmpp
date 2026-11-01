#include "casmpp.h"

/* ─────────────────────────────────────────────
   FUNCTION REGISTRY
   ───────────────────────────────────────────── */

/* Every function that is currently loaded. Entries are added when a script is
   loaded, in the order the definitions appear, and removed when it is freed.
   Lookups walk the list backwards, so a definition from a file that was "run"
   shadows one from the file that ran it. */
static Function functions[MAX_FUNCTIONS];
static int      function_count = 0;

/* ─────────────────────────────────────────────
   NAMES
   ───────────────────────────────────────────── */

static int is_name_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* Words the interpreter already gives a meaning to. A function with one of
   these names could never be called without breaking that meaning. */
static const char *reserved_words[] = {
    "if", "elif", "else", "while", "for", "end", "fn", "break",
    "out", "in", "run", "int", "str", "sml"
};

static int is_reserved(const char *name) {
    for (size_t i = 0; i < sizeof(reserved_words) / sizeof(reserved_words[0]); i++) {
        if (strcmp(reserved_words[i], name) == 0) return 1;
    }
    return 0;
}

static void fn_syntax_error(int line_num) {
    printf(COLOR_RED "Syntax error: 'fn' expects: fn <name>(<type> <name>, ...) on line %d" COLOR_RESET "\n", line_num);
}

/* ─────────────────────────────────────────────
   DEFINITIONS
   ───────────────────────────────────────────── */

/* Parse "fn <name>(<type> <name>, ...)" into out.
   Returns 1 on success, or 0 after reporting an error. */
static int parse_signature(const char *header, int line_num, Function *out) {
    memset(out, 0, sizeof(*out));

    const char *p = header + 2;
    while (*p == ' ' || *p == '\t') p++;

    if (!is_name_start(*p)) {
        fn_syntax_error(line_num);
        return 0;
    }

    int length = 0;
    while (is_name_char(*p)) {
        if (length < MAX_NAME_LEN - 1) out->name[length++] = *p;
        p++;
    }
    out->name[length] = '\0';

    if (is_reserved(out->name)) {
        printf(COLOR_RED "Error: '%s' is a reserved word, so it cannot name a function on line %d" COLOR_RESET "\n",
               out->name, line_num);
        return 0;
    }

    while (*p == ' ' || *p == '\t') p++;

    if (*p != '(') {
        fn_syntax_error(line_num);
        return 0;
    }
    p++;

    for (;;) {
        while (*p == ' ' || *p == '\t') p++;

        /* ")" closes the list, which also covers "fn ping()". */
        if (*p == ')') {
            p++;
            break;
        }

        if (out->param_count >= MAX_PARAMS) {
            printf(COLOR_RED "Error: function '%s' takes at most %d parameters on line %d" COLOR_RESET "\n",
                   out->name, MAX_PARAMS, line_num);
            return 0;
        }

        char type_name[16];
        int  type_length = 0;
        while (is_name_char(*p)) {
            if (type_length < 15) type_name[type_length++] = *p;
            p++;
        }
        type_name[type_length] = '\0';

        VarType type;
        if (strcmp(type_name, "int") == 0)       type = TYPE_INT;
        else if (strcmp(type_name, "str") == 0)  type = TYPE_STR;
        else if (strcmp(type_name, "sml") == 0)  type = TYPE_SML;
        else if (type_length == 0) {
            fn_syntax_error(line_num);
            return 0;
        } else {
            printf(COLOR_RED "Syntax error: unknown parameter type '%s' for '%s' on line %d (expected int, str or sml)" COLOR_RESET "\n",
                   type_name, out->name, line_num);
            return 0;
        }

        while (*p == ' ' || *p == '\t') p++;

        if (!is_name_start(*p)) {
            fn_syntax_error(line_num);
            return 0;
        }

        char param_name[MAX_NAME_LEN];
        int  param_length = 0;
        while (is_name_char(*p)) {
            if (param_length < MAX_NAME_LEN - 1) param_name[param_length++] = *p;
            p++;
        }
        param_name[param_length] = '\0';

        for (int i = 0; i < out->param_count; i++) {
            if (strcmp(out->params[i].name, param_name) == 0) {
                printf(COLOR_RED "Error: parameter '%s' is declared twice for '%s' on line %d" COLOR_RESET "\n",
                       param_name, out->name, line_num);
                return 0;
            }
        }

        out->params[out->param_count].type = type;
        snprintf(out->params[out->param_count].name, MAX_NAME_LEN, "%s", param_name);
        out->param_count++;

        while (*p == ' ' || *p == '\t') p++;

        if (*p == ',') {
            p++;
            continue;
        }

        if (*p == ')') {
            p++;
            break;
        }

        printf(COLOR_RED "Syntax error: expected ',' or ')' in the parameter list of '%s' on line %d" COLOR_RESET "\n",
               out->name, line_num);
        return 0;
    }

    /* A Python style trailing colon is allowed, like "if" and "for". */
    while (*p == ' ' || *p == '\t') p++;
    if (*p == ':') {
        p++;
        while (*p == ' ' || *p == '\t') p++;
    }

    if (*p != '\0') {
        printf(COLOR_RED "Syntax error: unexpected text '%s' after the definition of '%s' on line %d" COLOR_RESET "\n",
               p, out->name, line_num);
        return 0;
    }

    return 1;
}

/* Collect every function defined in a freshly loaded script.
   Definitions are only allowed at the top level of the file, so their position
   among the other lines does not matter — the whole file is scanned before any
   of it runs. A block that never closes is reported and stops the scan, since
   nothing after it can be trusted. */
void functions_register(Script *script, const char *dir) {
    int depth = 0;

    for (int i = 0; i < script->count; i++) {
        const char *text     = script->lines[i].text;
        int         line_num = script->lines[i].line_num;

        if (is_fn_header(text) && depth == 0) {
            int end = script_find_end(script, i);
            if (end < 0) {
                printf(COLOR_RED "Error: fn-block starting before line %d has no matching 'end'" COLOR_RESET "\n",
                       line_num);
                return;
            }

            Function parsed;
            if (parse_signature(text, line_num, &parsed)) {
                int duplicate = 0;

                for (int k = 0; k < function_count; k++) {
                    if (functions[k].script_id == script->id &&
                        strcmp(functions[k].name, parsed.name) == 0) {
                        printf(COLOR_RED "Error: function '%s' is already defined on line %d (first defined on line %d)" COLOR_RESET "\n",
                               parsed.name, line_num, functions[k].def_line);
                        duplicate = 1;
                        break;
                    }
                }

                if (!duplicate) {
                    if (function_count >= MAX_FUNCTIONS) {
                        printf(COLOR_RED "Error: too many functions (limit is %d)" COLOR_RESET "\n", MAX_FUNCTIONS);
                    } else {
                        parsed.script     = script;
                        parsed.script_id  = script->id;
                        parsed.body_start = i + 1;
                        parsed.body_end   = end;
                        parsed.def_line   = line_num;
                        snprintf(parsed.dir, sizeof(parsed.dir), "%s", dir);
                        functions[function_count++] = parsed;
                    }
                }
            }

            i = end;
            continue;
        }

        if (is_fn_header(text)) {
            printf(COLOR_RED "Error: functions can only be defined at the top level of a script (line %d)" COLOR_RESET "\n",
                   line_num);
        }

        if (is_block_opener(text)) {
            depth++;
        } else if (is_end_line(text) && depth > 0) {
            depth--;
        }
    }
}

/* Forget every function a script defined, so its lines can be freed and a
   later file can define the same names again. */
void functions_unregister_script(Script *script) {
    int kept = 0;

    for (int i = 0; i < function_count; i++) {
        if (functions[i].script_id == script->id) continue;

        if (kept != i) functions[kept] = functions[i];
        kept++;
    }

    function_count = kept;
}

/* Find a function by name, newest definition first. */
Function *find_function(const char *name) {
    for (int i = function_count - 1; i >= 0; i--) {
        if (strcmp(functions[i].name, name) == 0) return &functions[i];
    }
    return NULL;
}

/* ─────────────────────────────────────────────
   CALLS
   ───────────────────────────────────────────── */

/* Is this line shaped like a call — a bare name followed by parentheses, with
   nothing after the closing one? Anything else is left to the commands. */
int is_call_line(const char *line) {
    size_t length = strlen(line);
    if (length < 3 || line[length - 1] != ')') return 0;

    const char *open = strchr(line, '(');
    if (!open || open == line) return 0;

    for (const char *p = line; p < open; p++) {
        if (!is_name_char(*p)) return 0;
    }

    return is_name_start(line[0]);
}

/* Store one argument after trimming it, refusing empty ones and more than a
   function can ever take. */
static int store_argument(char *raw, const char *function_name,
                          char args[][MAX_STR_LEN], int *arg_count, int line_num) {
    while (*raw == ' ' || *raw == '\t') raw++;

    size_t length = strlen(raw);
    while (length > 0 && (raw[length - 1] == ' ' || raw[length - 1] == '\t')) length--;

    if (length == 0) {
        printf(COLOR_RED "Syntax error: empty argument in the call to '%s' on line %d" COLOR_RESET "\n",
               function_name, line_num);
        return 0;
    }

    if (*arg_count >= MAX_PARAMS) {
        printf(COLOR_RED "Error: '%s' is called with more than %d arguments on line %d" COLOR_RESET "\n",
               function_name, MAX_PARAMS, line_num);
        return 0;
    }

    if (length > (size_t)(MAX_STR_LEN - 1)) length = MAX_STR_LEN - 1;
    memcpy(args[*arg_count], raw, length);
    args[*arg_count][length] = '\0';
    (*arg_count)++;
    return 1;
}

/* Split "name(argument, argument)" into the name and its arguments.
   Arguments are kept as written (trimmed), and a quoted string may contain
   commas. Returns 1 on success, or 0 after reporting an error. */
int parse_function_call(const char *line, char *name, char args[][MAX_STR_LEN],
                        int *arg_count, int line_num) {
    *arg_count = 0;

    const char *open = strchr(line, '(');
    if (!open) {
        printf(COLOR_RED "Syntax error: expected '(' in the call on line %d" COLOR_RESET "\n", line_num);
        return 0;
    }

    size_t name_length = (size_t)(open - line);
    if (name_length >= MAX_NAME_LEN) name_length = MAX_NAME_LEN - 1;
    memcpy(name, line, name_length);
    name[name_length] = '\0';

    const char *close = line + strlen(line) - 1; /* the final ')' */

    /* "name()" and "name(   )" take no arguments at all. */
    int only_space = 1;
    for (const char *p = open + 1; p < close; p++) {
        if (*p != ' ' && *p != '\t') {
            only_space = 0;
            break;
        }
    }
    if (only_space) return 1;

    char current[MAX_STR_LEN * 2];
    int  length = 0;
    int  quoted = 0;

    for (const char *p = open + 1; p < close; p++) {
        char c = *p;

        if (quoted) {
            if (length < (int)sizeof(current) - 1) current[length++] = c;
            if (c == '"') quoted = 0;
            continue;
        }

        if (c == '"') {
            quoted = 1;
            if (length < (int)sizeof(current) - 1) current[length++] = c;
            continue;
        }

        if (c == '(') {
            printf(COLOR_RED "Syntax error: function arguments must be values or variables, not expressions, on line %d" COLOR_RESET "\n",
                   line_num);
            return 0;
        }

        if (c == ',') {
            current[length] = '\0';
            if (!store_argument(current, name, args, arg_count, line_num)) return 0;
            length = 0;
            continue;
        }

        if (length < (int)sizeof(current) - 1) current[length++] = c;
    }

    if (quoted) {
        printf(COLOR_RED "Syntax error: unterminated string in the call to '%s' on line %d" COLOR_RESET "\n",
               name, line_num);
        return 0;
    }

    current[length] = '\0';
    return store_argument(current, name, args, arg_count, line_num);
}