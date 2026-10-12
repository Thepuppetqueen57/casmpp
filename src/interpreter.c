#include "casmpp.h"

/* ─────────────────────────────────────────────
   IF / ELIF / ELSE / END BLOCK
   ───────────────────────────────────────────── */

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