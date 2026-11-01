#include "casmpp.h"

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
       or:  out <expression>
   line_num is used only for error messages. */
void cmd_output(char *args, int line_num) {
    char *trimmed = trim_whitespace(args);
    size_t length = strlen(trimmed);

    if (length >= 2 && trimmed[0] == '"' && trimmed[length - 1] == '"') {
        /* It's a string literal — print the text between the quotes. */
        trimmed[length - 1] = '\0'; /* remove closing quote */
        printf("%s\n", trimmed + 1); /* skip opening quote  */
        free(trimmed);
        return;
    }

    /* A plain variable prints as it always has, text included. */
    Variable *var = find_variable(trimmed);
    if (var) {
        if      (var->type == TYPE_INT) printf("%d\n", var->value.intValue);
        else if (var->type == TYPE_STR) printf("%s\n", var->value.strValue);
        else                            printf("%d\n", var->value.smlValue);
        free(trimmed);
        return;
    }

    /* A bare word that is not a variable keeps the message it always had. */
    int bare_name = trimmed[0] != '\0' && !isdigit((unsigned char)trimmed[0]);
    for (const char *p = trimmed; *p != '\0'; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') {
            bare_name = 0;
            break;
        }
    }

    if (bare_name) {
        printf(COLOR_RED "Error: unknown variable '%s' on line %d" COLOR_RESET "\n", trimmed, line_num);
        free(trimmed);
        return;
    }

    /* Anything else is an expression, so "out 5 + 3" and "out double(2)"
       print what they work out to. */
    Operand value;
    if (eval_expression(trimmed, line_num, &value)) {
        if (value.is_text) printf("%s\n", value.text);
        else               printf("%d\n", value.int_value);
    }

    free(trimmed);
}

/* ─────────────────────────────────────────────
   COMMAND DISPATCHER
   ───────────────────────────────────────────── */

/* Parse the first word of a line to decide which command to run.
   The line comes from the loaded script, which runs again on every pass of a
   loop, so it is copied before any command gets a chance to modify it. */
void run_command(const char *line, int line_num) {
    char *copy = strdup(line);
    if (!copy) {
        fprintf(stderr, COLOR_RED "run_command: strdup failed: %s" COLOR_RESET "\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Everything after the first space is the argument; the command word is
       what comes before it. */
    char *space   = strchr(copy, ' ');
    char *args    = space ? space + 1 : copy + strlen(copy);
    char *command = strtok(copy, " ");

    if (command == NULL) {
        free(copy);
        return;
    }

    if (strcmp(command, "out") == 0) {
        cmd_output(args, line_num);

    } else if (strcmp(command, "in") == 0) {
        cmd_input(args);

    } else if (strcmp(command, "run") == 0) {
        cmd_run(args, line_num);

    } else {
        printf(COLOR_RED "Error: unknown command '%s' on line %d" COLOR_RESET "\n", command, line_num);
    }

    free(copy);
}