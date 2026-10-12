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