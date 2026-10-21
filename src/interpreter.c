#include "casmpp.h"

/* ─────────────────────────────────────────────
   BLOCK EXECUTION
   ───────────────────────────────────────────── */

/* Directory of the file being interpreted right now, including its trailing
   separator ("" when the file was named without a directory). The "run"
   command resolves the paths it is given against this, so a script runs the
   files sitting next to it no matter where casmpp was started from. */
static char current_dir[MAX_STR_LEN] = "";

/* How many files are being executed right now, so a script that runs itself —
   directly or through another file — is reported instead of crashing. */
static int run_depth = 0;

/* How deeply blocks are nested right now. Guards the C stack against a script
   nested thousands of levels deep. */
static int block_depth = 0;

static Flow exec_range(Script *script, int start, int end, int in_loop);

/* ─────────────────────────────────────────────
   BLOCK HELPERS
   ───────────────────────────────────────────── */

/* Has the nesting limit been reached? Reports the error once, and the caller
   then skips the whole block instead of descending into it. */
static int block_limit_reached(int line_num) {
    if (block_depth < MAX_BLOCK_DEPTH) return 0;

    printf(COLOR_RED "Error: blocks are nested too deeply (limit is %d) on line %d" COLOR_RESET "\n",
           MAX_BLOCK_DEPTH, line_num);
    return 1;
}

/* Index of the line after the block opened at opener, or the end of the script
   when the block is never closed. */
static int block_end_or_count(Script *script, int opener) {
    int end = script_find_end(script, opener);
    return end < 0 ? script->count : end + 1;
}

/* Execute one branch of an if chain, from *start up to end, then move *start
   to end. A branch that is not being taken does nothing. */
static Flow exec_segment(Script *script, int *start, int end, int run, int in_loop) {
    int from = *start;
    *start = end;

    if (!run) return FLOW_NORMAL;
    return exec_range(script, from, end, in_loop);
}

/* ─────────────────────────────────────────────
   IF / ELIF / ELSE
   ───────────────────────────────────────────── */

/* Execute the if chain whose header is at script->lines[opener].
   *next is left on the line after the matching "end".

   Format in script:
       if <value> <operator> <value>
           ...commands to run when this condition is true...
       elif <value> <operator> <value>
           ...commands to run when nothing before this matched...
       else
           ...commands to run when no condition matched...
       end

   Any number of "elif" branches may sit between the "if" and the "else".
   Branches are separated at this block's own level only: nested if/while/for
   blocks are skipped whole, so their own elif and else lines belong to them.

   If a branch header is invalid (bad syntax, unknown variable, unknown
   operator) the error is reported once and no branch of the chain runs — but
   the block is still consumed up to its "end" so the body is never executed as
   top-level commands. */
static Flow exec_if(Script *script, int opener, int in_loop, int *next) {
    int line_num = script->lines[opener].line_num;

    if (block_limit_reached(line_num)) {
        *next = block_end_or_count(script, opener);
        return FLOW_NORMAL;
    }

    int body_end    = script_find_end(script, opener);
    int missing_end = 0;

    /* A block that never closes still runs its branch, and the missing "end"
       is reported afterwards. That keeps the order the streaming interpreter
       produced: the body ran as it was read, and the error came at the end of
       the file. */
    if (body_end < 0) {
        body_end    = script->count;
        missing_end = 1;
    }

    /* Chain state:
         chain_taken  — a branch has already matched, so later ones must not run
         chain_broken — a header was invalid, so no branch runs at all
         run_branch   — the branch currently being walked runs its lines? */
    int chain_taken  = 0;
    int chain_broken = 0;
    int run_branch   = 0;

    int truth = 0;
    if (evaluate_condition(script->lines[opener].text, line_num, &truth)) {
        run_branch = truth;
        if (run_branch) chain_taken = 1;
    } else {
        chain_broken = 1;
    }

    int segment_start = opener + 1;
    int i             = opener + 1;
    Flow flow         = FLOW_NORMAL;

    block_depth++;

    while (i < body_end && flow == FLOW_NORMAL) {
        const char *text = script->lines[i].text;

        /* Nested blocks are jumped over in one step. */
        if (is_block_opener(text)) {
            i = block_end_or_count(script, i);
            continue;
        }

        if (is_elif_header(text)) {
            flow = exec_segment(script, &segment_start, i, run_branch, in_loop);
            if (flow == FLOW_BREAK) break;

            int elif_truth = 0;
            if (evaluate_condition(text, script->lines[i].line_num, &elif_truth)) {
                run_branch = !chain_broken && !chain_taken && elif_truth;
                if (run_branch) chain_taken = 1;
            } else {
                chain_broken = 1;
                run_branch   = 0;
            }

            segment_start = i + 1;
            i++;
            continue;
        }

        if (is_else_header(text)) {
            flow = exec_segment(script, &segment_start, i, run_branch, in_loop);
            if (flow == FLOW_BREAK) break;

            run_branch = !chain_broken && !chain_taken;
            if (run_branch) chain_taken = 1;

            segment_start = i + 1;
            i++;
            continue;
        }

        i++;
    }

    /* The last branch runs from wherever it started up to "end". */
    if (flow == FLOW_NORMAL) {
        flow = exec_segment(script, &segment_start, body_end, run_branch, in_loop);
    }

    block_depth--;

    if (missing_end) {
        printf(COLOR_RED "Error: if-block starting before line %d has no matching 'end'" COLOR_RESET "\n", script->last_line_num);
    }

    *next = missing_end ? script->count : body_end + 1;
    return flow;
}

/* ─────────────────────────────────────────────
   WHILE
   ───────────────────────────────────────────── */

/* Execute a while loop.
   Format in script:
       while <value> <operator> <value>
           ...commands to run on every pass...
       end

   The condition accepts exactly what an if accepts, so "while var < 500",
   "while name != \"stop\"" and "while 0 >= 1" all work. A "break" in the body
   ends the loop immediately. */
static Flow exec_while(Script *script, int opener, int *next) {
    int line_num = script->lines[opener].line_num;

    if (block_limit_reached(line_num)) {
        *next = block_end_or_count(script, opener);
        return FLOW_NORMAL;
    }

    int body_end = script_find_end(script, opener);
    if (body_end < 0) {
        printf(COLOR_RED "Error: while-block starting before line %d has no matching 'end'" COLOR_RESET "\n", script->last_line_num);
        *next = script->count;
        return FLOW_NORMAL;
    }

    Flow flow = FLOW_NORMAL;

    block_depth++;

    while (1) {
        int truth = 0;

        /* An invalid condition is reported once and the body never runs, so a
           typo cannot turn into an endless loop. */
        if (!evaluate_condition(script->lines[opener].text, line_num, &truth)) break;
        if (!truth) break;

        flow = exec_range(script, opener + 1, body_end, 1);
        if (flow == FLOW_BREAK) {
            flow = FLOW_NORMAL;
            break;
        }
    }

    block_depth--;

    *next = body_end + 1;
    return flow;
}

/* ─────────────────────────────────────────────
   FOR
   ───────────────────────────────────────────── */

/* Parse the header of a for loop.
   Format:  for <name> = <start>-<end>
   Both ends may be a number or a variable, and a Python style trailing colon
   is allowed. Returns 1 on success, or 0 after reporting an error. */
static int parse_for_header(const char *header, int line_num,
                            char *name, char *start_token, char *end_token) {
    const char *p = header + 3; /* past "for" */
    while (*p == ' ' || *p == '\t') p++;

    int i = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '=') {
        if (i < MAX_NAME_LEN - 1) name[i++] = *p;
        p++;
    }
    name[i] = '\0';

    while (*p == ' ' || *p == '\t') p++;

    if (name[0] == '\0' || *p != '=') {
        printf(COLOR_RED "Syntax error: 'for' expects: for <name> = <start>-<end> on line %d" COLOR_RESET "\n", line_num);
        return 0;
    }
    p++; /* past '=' */

    char range[MAX_STR_LEN];
    snprintf(range, sizeof(range), "%s", p);

    size_t length = strlen(range);
    while (length > 0 && (range[length - 1] == ' ' || range[length - 1] == '\t')) range[--length] = '\0';
    if (length > 0 && range[length - 1] == ':') range[--length] = '\0';

    /* Drop the spaces first, so "0 - 5" splits exactly like "0-5", then split
       at the last '-' that is not the sign of a negative start: "-5-5" reads
       as -5 up to 5. */
    char compact[MAX_STR_LEN];
    int  written = 0;
    for (int k = 0; range[k] != '\0'; k++) {
        if (range[k] != ' ' && range[k] != '\t') compact[written++] = range[k];
    }
    compact[written] = '\0';

    int separator = -1;
    for (int k = 1; compact[k] != '\0'; k++) {
        if (compact[k] == '-') separator = k;
    }

    if (separator < 0) {
        printf(COLOR_RED "Syntax error: 'for' expects a range like 0-5 on line %d" COLOR_RESET "\n", line_num);
        return 0;
    }

    compact[separator] = '\0';
    snprintf(start_token, MAX_STR_LEN, "%s", compact);
    snprintf(end_token, MAX_STR_LEN, "%s", compact + separator + 1);

    if (start_token[0] == '\0' || end_token[0] == '\0') {
        printf(COLOR_RED "Syntax error: 'for' expects a range like 0-5 on line %d" COLOR_RESET "\n", line_num);
        return 0;
    }

    return 1;
}

/* Execute a for loop.
   Format in script:
       for i = 0-5
           ...commands to run on every pass...
       end

   i takes 0, 1, 2, 3 and 4 — the end value is not included, so 0-5 runs five
   times. After a normal finish the loop variable is left on the end value,
   like a counting loop in C; after a "break" it keeps the value it had. */
static Flow exec_for(Script *script, int opener, int *next) {
    int line_num = script->lines[opener].line_num;

    if (block_limit_reached(line_num)) {
        *next = block_end_or_count(script, opener);
        return FLOW_NORMAL;
    }

    int body_end = script_find_end(script, opener);
    if (body_end < 0) {
        printf(COLOR_RED "Error: for-block starting before line %d has no matching 'end'" COLOR_RESET "\n", script->last_line_num);
        *next = script->count;
        return FLOW_NORMAL;
    }

    char name[MAX_NAME_LEN];
    char start_token[MAX_STR_LEN];
    char end_token[MAX_STR_LEN];

    if (!parse_for_header(script->lines[opener].text, line_num, name, start_token, end_token)) {
        *next = body_end + 1;
        return FLOW_NORMAL;
    }

    Operand start;
    Operand end;
    if (!resolve_operand(start_token, 0, line_num, &start) ||
        !resolve_operand(end_token, 0, line_num, &end)) {
        *next = body_end + 1;
        return FLOW_NORMAL;
    }

    if (start.is_text || end.is_text) {
        printf(COLOR_RED "Error: the range of a for loop must be numbers on line %d" COLOR_RESET "\n", line_num);
        *next = body_end + 1;
        return FLOW_NORMAL;
    }

    int  broke = 0;
    Flow flow  = FLOW_NORMAL;

    block_depth++;

    for (int value = start.int_value; value < end.int_value; value++) {
        char text[32];
        snprintf(text, sizeof(text), "%d", value);
        add_variable(name, TYPE_INT, text);

        flow = exec_range(script, opener + 1, body_end, 1);
        if (flow == FLOW_BREAK) {
            broke = 1;
            flow  = FLOW_NORMAL;
            break;
        }
    }

    block_depth--;

    if (!broke) {
        char text[32];
        snprintf(text, sizeof(text), "%d", end.int_value);
        add_variable(name, TYPE_INT, text);
    }

    *next = body_end + 1;
    return flow;
}

/* ─────────────────────────────────────────────
   LINE EXECUTION
   ───────────────────────────────────────────── */

/* Is this line just the word "break"? */
static int is_break_line(const char *line) {
    return strcmp(line, "break") == 0;
}

/* Execute the lines at [start, end).
   Returns FLOW_BREAK when a "break" in this range should stop the enclosing
   loop, and FLOW_NORMAL otherwise. in_loop says whether a "break" here has a
   loop to stop; when it does not, the mistake is reported instead. */
static Flow exec_range(Script *script, int start, int end, int in_loop) {
    int i = start;

    while (i < end) {
        const char *text     = script->lines[i].text;
        int         line_num = script->lines[i].line_num;

        /* ── If statement ─────────────────────────────────────────────
           Hands control over to exec_if(), which walks the branches and
           leaves i on the line after the matching "end".            */
        if (is_if_header(text)) {
            Flow flow = exec_if(script, i, in_loop, &i);
            if (flow == FLOW_BREAK) return flow;
            continue;
        }

        /* ── While loop ─────────────────────────────────────────────── */
        if (is_while_header(text)) {
            Flow flow = exec_while(script, i, &i);
            if (flow == FLOW_BREAK) return flow;
            continue;
        }

        /* ── For loop ───────────────────────────────────────────────── */
        if (is_for_header(text)) {
            Flow flow = exec_for(script, i, &i);
            if (flow == FLOW_BREAK) return flow;
            continue;
        }

        /* ── Break ────────────────────────────────────────────────────
           Inside a loop this stops the innermost one. Anywhere else it is a
           mistake worth saying out loud.                            */
        if (is_break_line(text)) {
            if (in_loop) return FLOW_BREAK;

            printf(COLOR_RED "Error: 'break' can only be used inside a loop on line %d" COLOR_RESET "\n", line_num);
            i++;
            continue;
        }

        /* ── A stray end ──────────────────────────────────────────────
           Every block consumed its own "end" already, so there is nothing
           here for this one to close.                               */
        if (is_end_line(text)) {
            printf(COLOR_RED "Error: 'end' without a matching block on line %d" COLOR_RESET "\n", line_num);
            i++;
            continue;
        }

        /* ── Variable declaration or assignment ─────────────────────
           Format:  <type> <name> = <value>
           Example: int score = 0
                    int score += 1
                    str greeting = "hello"
                    sml flag = 1                                        */
        if (is_declaration_line(text)) {
            exec_declaration(text, line_num);
            i++;
            continue;
        }

        /* ── Everything else is treated as a command (out, in, run, …) ─ */
        run_command(text, line_num);
        i++;
    }

    return FLOW_NORMAL;
}

/* Execute a loaded script from the first line to the last. */
static void exec_script(Script *script) {
    exec_range(script, 0, script->count, 0);
}

/* Execute a loaded script with filename's directory current, putting the
   previous directory back afterwards. */
static void run_script(Script *script, const char *filename) {
    char saved_dir[MAX_STR_LEN];
    snprintf(saved_dir, sizeof(saved_dir), "%s", current_dir);

    path_directory(filename, current_dir, sizeof(current_dir));

    exec_script(script);

    snprintf(current_dir, sizeof(current_dir), "%s", saved_dir);
}

/* Open a .casmpp file and execute it line by line. */
void interpret_file(const char *filename) {
    Script script;

    if (!script_load(filename, &script)) {
        printf(COLOR_RED "Error: could not open file '%s'" COLOR_RESET "\n", filename);
        return;
    }

    run_script(&script, filename);
    script_free(&script);
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

    Script script;
    if (!script_load(full_path, &script)) {
        printf(COLOR_RED "Error: could not open file '%s' on line %d" COLOR_RESET "\n", full_path, line_num);
        return;
    }

    if (strcmp(extension, ".casm") == 0) {
        printf(COLOR_YELLOW "Warning: Running a .casm file" COLOR_RESET "\n");
        printf(COLOR_YELLOW ".casm files are deprecated, please use .casmpp instead." COLOR_RESET "\n");
    }

    run_depth++;
    run_script(&script, full_path);
    run_depth--;

    script_free(&script);
}