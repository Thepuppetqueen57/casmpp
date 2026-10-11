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

/* ─────────────────────────────────────────────
   STRING UTILITY
   ───────────────────────────────────────────── */

/* Return a newly-allocated copy of str with:
     - leading whitespace removed
     - trailing whitespace AND trailing semicolons removed
   Caller is responsible for free()-ing the result. */
char *trim_whitespace(const char *str) {
    /* Work on a duplicate so we don't modify the caller's string. */
    char *copy = strdup(str);
    if (!copy) {
        fprintf(stderr, COLOR_RED "trim_whitespace: strdup failed: %s" COLOR_RESET "\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Skip past any leading spaces/tabs/newlines. */
    char *start = copy;
    while (isspace((unsigned char)*start)) {
        start++;
    }

    /* If we hit the end of the string it was all whitespace. */
    if (*start == '\0') {
        free(copy);
        return strdup("");
    }

    /* Walk back from the end, stripping trailing whitespace and semicolons. */
    char *end = start + strlen(start) - 1;
    while (end > start && (isspace((unsigned char)*end) || *end == ';')) {
        end--;
    }
    *(end + 1) = '\0'; /* null-terminate the trimmed region */

    char *trimmed = strdup(start);
    if (!trimmed) {
        fprintf(stderr, COLOR_RED "trim_whitespace: second strdup failed: %s" COLOR_RESET "\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    free(copy);
    return trimmed;
}

/* ─────────────────────────────────────────────
   BUILT-IN COMMANDS
   ───────────────────────────────────────────── */

/* Handle the "in" command, which prompts the user for input and stores it.
   Format in script:  in <type> <varname> "prompt text"
   Example:           in int age "Enter your age: " */
void cmd_input(char *args) {
    char *saveptr;

    char *type_str = strtok_r(args,   " ", &saveptr); /* e.g. "int"          */
    char *var_name = strtok_r(NULL,   " ", &saveptr); /* e.g. "age"          */
    char *prompt   = saveptr;                         /* e.g. "Enter your age: " */

    if (!type_str || !var_name || !prompt) {
        printf(COLOR_RED "Syntax error: 'in' expects: in <type> <name> \"prompt\"" COLOR_RESET "\n");
        return;
    }

    /* Strip the surrounding quotes from the prompt string. */
    prompt = strtok(prompt, "\"");
    if (!prompt) {
        printf(COLOR_RED "Syntax error: prompt string must be surrounded by quotes" COLOR_RESET "\n");
        return;
    }

    /* Show the prompt and read a line of input from the user. */
    printf("%s", prompt);
    char input[MAX_STR_LEN];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        printf(COLOR_RED "Error: could not read input" COLOR_RESET "\n");
        return;
    }
    input[strcspn(input, "\n")] = '\0'; /* strip the trailing newline */

    /* Validate and store the input according to the requested type. */
    if (strcmp(type_str, "str") == 0) {
        add_variable(var_name, TYPE_STR, input);

    } else if (strcmp(type_str, "int") == 0) {
        char *endptr;
        long num = strtol(input, &endptr, 10);
        if (*endptr != '\0') {
            printf(COLOR_RED "Error: '%s' is not a valid integer for variable '%s'" COLOR_RESET "\n", input, var_name);
            return;
        }
        char num_str[32];
        snprintf(num_str, sizeof(num_str), "%ld", num);
        add_variable(var_name, TYPE_INT, num_str);

    } else if (strcmp(type_str, "sml") == 0) {
        if (strcmp(input, "0") == 0 || strcmp(input, "1") == 0) {
            add_variable(var_name, TYPE_SML, input);
        } else {
            printf(COLOR_RED "Error: sml variable '%s' must be 0 or 1, got '%s'" COLOR_RESET "\n", var_name, input);
        }

    } else {
        printf(COLOR_RED "Error: unknown type '%s' in 'in' command" COLOR_RESET "\n", type_str);
    }
}

/* Handle the "out" command, which prints a value to the screen.
   Format:  out "some literal text"
       or:  out <varname>
   line_num is used only for error messages. */
void cmd_output(char *args, int line_num) {
    char *trimmed = trim_whitespace(args);

    if (trimmed[0] == '"' && trimmed[strlen(trimmed) - 1] == '"') {
        /* It's a string literal — print the text between the quotes. */
        trimmed[strlen(trimmed) - 1] = '\0'; /* remove closing quote */
        printf("%s\n", trimmed + 1);          /* skip opening quote  */
    } else {
        /* It's a variable name — look it up and print its value. */
        Variable *var = find_variable(trimmed);
        if (var) {
            if      (var->type == TYPE_INT) printf("%d\n", var->value.intValue);
            else if (var->type == TYPE_STR) printf("%s\n", var->value.strValue);
            else if (var->type == TYPE_SML) printf("%d\n", var->value.smlValue);
        } else {
            printf(COLOR_RED "Error: unknown variable '%s' on line %d" COLOR_RESET "\n", trimmed, line_num);
        }
    }

    free(trimmed);
}

/* ─────────────────────────────────────────────
   COMMAND DISPATCHER
   ───────────────────────────────────────────── */

/* Parse the first word of a line to decide which command to run. */
void run_command(char *line, int line_num) {
    /* strtok modifies the string, so grab the command word first. */
    char *command = strtok(line, " ");
    if (command == NULL) return;

    if (strcmp(command, "out") == 0) {
        /* Everything after "out " is the argument. */
        char *args = line + strlen(command) + 1;
        cmd_output(args, line_num);

    } else if (strcmp(command, "in") == 0) {
        /* Everything after "in " is the argument (type, name, prompt). */
        cmd_input(line + strlen(command) + 1);

    } else {
        printf(COLOR_RED "Error: unknown command '%s' on line %d" COLOR_RESET "\n", command, line_num);
    }
}

/* ─────────────────────────────────────────────
   IF / ELIF / ELSE / END BLOCK
   ───────────────────────────────────────────── */

/* One side of a comparison. A side is either a whole number or a piece of
   text; which one it is decides how the two sides are compared. */
typedef struct {
    int  is_text;
    int  int_value;
    char text[MAX_STR_LEN];
} Operand;

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

/* Process an if-block read from the file.
   Format in script:
       if <value> <operator> <value>
           ...commands to run when this condition is true...
       elif <value> <operator> <value>
           ...commands to run when nothing before this matched...
       else
           ...commands to run when no condition matched...
       end

   Any number of "elif" branches may sit between the "if" and the "else".
   Supported operators: ==, !=, >, <, >= and <=. Both sides of a comparison may
   be a variable, a number, or a quoted string.
   line_num is updated as new lines are read so error messages stay accurate.

   If a branch header is invalid (bad syntax, unknown variable, unknown
   operator) the error is reported once and no branch of the chain runs — but
   the block is still consumed up to its "end" so the body is never executed as
   top-level commands. */
void process_if_block(FILE *file, char *condition_line, int *line_num) {
    /* Chain state:
         chain_taken  — a branch has already matched, so later ones must not run
         chain_broken — a header was invalid, so no branch runs at all
         run_branch   — should the body lines we are reading run right now?  */
    int chain_taken  = 0;
    int chain_broken = 0;
    int run_branch   = 0;

    /* Evaluate the "if" header that brought us here. */
    int truth = 0;
    if (evaluate_condition(condition_line, *line_num, &truth)) {
        run_branch = truth;
        if (run_branch) chain_taken = 1;
    } else {
        chain_broken = 1;
    }

    int  found_end = 0;
    char line[MAX_STR_LEN];

    /* Read lines until we hit "end" (or run out of file). */
    while (fgets(line, sizeof(line), file)) {
        (*line_num)++;
        char *trimmed = trim_whitespace(line);

        if (strcmp(trimmed, "end") == 0) {
            found_end = 1;
            free(trimmed);
            break;
        }

        /* "else" runs only when nothing before it matched. */
        if (is_keyword_line(trimmed, "else")) {
            run_branch = !chain_broken && !chain_taken;
            if (run_branch) chain_taken = 1;
            free(trimmed);
            continue;
        }

        /* "elif" starts another condition in the same chain. */
        if (strncmp(trimmed, "elif", 4) == 0 &&
            (trimmed[4] == ' ' || trimmed[4] == '\t' || trimmed[4] == ':' || trimmed[4] == '\0')) {
            int elif_truth = 0;
            if (evaluate_condition(trimmed, *line_num, &elif_truth)) {
                run_branch = !chain_broken && !chain_taken && elif_truth;
                if (run_branch) chain_taken = 1;
            } else {
                chain_broken = 1;
                run_branch   = 0;
            }
            free(trimmed);
            continue;
        }

        if (run_branch) {
            run_command(trimmed, *line_num);
        }

        free(trimmed);
    }

    if (!found_end) {
        printf(COLOR_RED "Error: if-block starting before line %d has no matching 'end'" COLOR_RESET "\n", *line_num);
    }
}

/* ─────────────────────────────────────────────
   MAIN INTERPRETER LOOP
   ───────────────────────────────────────────── */

/* Open a .casm++ file and execute it line by line. */
void interpret_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf(COLOR_RED "Error: could not open file '%s'" COLOR_RESET "\n", filename);
        return;
    }

    char line[MAX_STR_LEN];
    int  line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char *trimmed = trim_whitespace(line);

        /* Skip blank lines and comment lines (lines starting with #). */
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            free(trimmed);
            continue;
        }

        /* ── Variable declaration ─────────────────────────────────────
           Format:  <type> <name> = <value>
           Example: int score = 0
                    str greeting = "hello"
                    sml flag = 1                                        */
        if (strncmp(trimmed, "int ", 4) == 0 ||
            strncmp(trimmed, "str ", 4) == 0 ||
            strncmp(trimmed, "sml ", 4) == 0) {

            char *type_str = strtok(trimmed, " "); /* "int" / "str" / "sml" */
            char *var_name = strtok(NULL,   " "); /* variable name          */
                             strtok(NULL,   " "); /* skip the "=" sign      */
            char *value    = strtok(NULL,  "\""); /* value (strips quotes)  */

            if (value) {
                VarType type;
                if      (strcmp(type_str, "int") == 0) type = TYPE_INT;
                else if (strcmp(type_str, "str") == 0) type = TYPE_STR;
                else                                    type = TYPE_SML;
                add_variable(var_name, type, value);
            }

        /* ── If statement ─────────────────────────────────────────────
           Hands control over to process_if_block(), which reads lines from
           the file until it finds the matching "end" — taking in any elif
           and else branches along the way.                            */
        } else if (is_keyword_line(trimmed, "if") || strncmp(trimmed, "if ", 3) == 0) {
            process_if_block(file, trimmed, &line_num);

        /* ── Everything else is treated as a command (out, in, …) ─── */
        } else {
            run_command(trimmed, line_num);
        }

        free(trimmed);
    }

    fclose(file);
}

/* ─────────────────────────────────────────────
   ENTRY POINT
   ───────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: casmpp <file.casmpp>\n");
        return 1;
    }

    const char *filename = argv[1];

    /* Find the file extension (the last '.' in the filename). */
    const char *extension = strrchr(filename, '.');

    if (extension == NULL) {
        printf(COLOR_RED "Error: file has no extension — please provide a .casmpp file" COLOR_RESET "\n");
        return 1;
    }

    if (strcmp(extension, ".casm") == 0) {
        printf(COLOR_YELLOW "Warning: Running a .casm file" COLOR_RESET "\n");
        printf(COLOR_YELLOW ".casm files are deprecated, please use .casmpp instead." COLOR_RESET "\n");
    }

    if (strcmp(extension, ".casmpp") != 0 && strcmp(extension, ".casm") != 0) {
        printf(COLOR_RED "Error: '%s' is not a valid file extension" COLOR_RESET "\n", filename);
        return 1;
    }

    interpret_file(filename);
    return 0;
}