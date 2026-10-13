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

/* Directory of the file being interpreted right now, including its trailing
   separator ("" when the file was named without a directory). The "run"
   command resolves the paths it is given against this, so a script runs the
   files sitting next to it no matter where casmpp was started from. */
static char current_dir[MAX_STR_LEN] = "";

/* How many files are being executed right now, so a script that runs itself —
   directly or through another file — is reported instead of crashing. */
static int run_depth = 0;

/* Execute an open script file, line by line. */
static void interpret_stream(FILE *file) {
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
}

/* Execute an open script with filename's directory current, putting the
   previous directory back afterwards. */
static void run_stream(FILE *file, const char *filename) {
    char saved_dir[MAX_STR_LEN];
    snprintf(saved_dir, sizeof(saved_dir), "%s", current_dir);

    path_directory(filename, current_dir, sizeof(current_dir));

    interpret_stream(file);

    snprintf(current_dir, sizeof(current_dir), "%s", saved_dir);
}

/* Open a .casm++ file and execute it line by line. */
void interpret_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf(COLOR_RED "Error: could not open file '%s'" COLOR_RESET "\n", filename);
        return;
    }

    run_stream(file, filename);
    fclose(file);
}

/* ─────────────────────────────────────────────
   RUNNING ANOTHER FILE (the "run" command)
   ───────────────────────────────────────────── */

/* Handle the "run" command, which executes another file in place of the run
   line, so both files share one set of variables.
   Format in script:
       run "otherfile.casmpp";
       run folder/otherotherfile.casm;

   The path is looked up relative to the file the "run" is written in — not
   relative to the directory casmpp was started from — and a .casm file runs
   just like a .casmpp one. line_num points at the "run" line for errors. */
void cmd_run(char *args, int line_num) {
    char *trimmed = trim_whitespace(args);
    char  path[MAX_STR_LEN];

    /* A quoted path may contain spaces, and nothing may follow the closing
       quote. Without quotes the rest of the line is taken as written. */
    if (trimmed[0] == '"') {
        char *closing = strchr(trimmed + 1, '"');
        if (!closing) {
            printf(COLOR_RED "Syntax error: unterminated file name in 'run' on line %d" COLOR_RESET "\n", line_num);
            free(trimmed);
            return;
        }

        *closing = '\0';
        snprintf(path, sizeof(path), "%s", trimmed + 1);

        char *trailing = trim_whitespace(closing + 1);
        if (trailing[0] != '\0') {
            printf(COLOR_RED "Syntax error: unexpected text '%s' after file name in 'run' on line %d" COLOR_RESET "\n", trailing, line_num);
            free(trailing);
            free(trimmed);
            return;
        }
        free(trailing);
    } else {
        snprintf(path, sizeof(path), "%s", trimmed);
    }

    free(trimmed);

    if (path[0] == '\0') {
        printf(COLOR_RED "Syntax error: 'run' expects a file name on line %d (example: run \"otherfile.casmpp\")" COLOR_RESET "\n", line_num);
        return;
    }

    /* Only scripts can be run. */
    const char *extension = strrchr(path, '.');
    if (!extension || (strcmp(extension, ".casmpp") != 0 && strcmp(extension, ".casm") != 0)) {
        printf(COLOR_RED "Error: '%s' is not a valid file extension for 'run' on line %d (expected .casmpp or .casm)" COLOR_RESET "\n", path, line_num);
        return;
    }

    if (run_depth >= MAX_RUN_DEPTH) {
        printf(COLOR_RED "Error: 'run' files are nested too deeply (limit is %d) on line %d" COLOR_RESET "\n", MAX_RUN_DEPTH, line_num);
        return;
    }

    /* Resolve the path against the directory of the file we are running. */
    char full_path[MAX_STR_LEN * 2];
    join_path(current_dir, path, full_path, sizeof(full_path));

    FILE *file = fopen(full_path, "r");
    if (!file) {
        printf(COLOR_RED "Error: could not open file '%s' on line %d" COLOR_RESET "\n", full_path, line_num);
        return;
    }

    if (strcmp(extension, ".casm") == 0) {
        printf(COLOR_YELLOW "Warning: Running a .casm file" COLOR_RESET "\n");
        printf(COLOR_YELLOW ".casm files are deprecated, please use .casmpp instead." COLOR_RESET "\n");
    }

    run_depth++;
    run_stream(file, full_path);
    run_depth--;

    fclose(file);
}