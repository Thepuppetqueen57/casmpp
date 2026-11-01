#include "casmpp.h"

/* ─────────────────────────────────────────────
   BLOCK HEADERS
   ───────────────────────────────────────────── */

/* These predicates are the only definition of what opens and closes a block.
   The interpreter dispatches on them and script_find_end counts them, so the
   two can never disagree about where a block ends. */

int is_if_header(const char *line) {
    return is_keyword_line(line, "if") || strncmp(line, "if ", 3) == 0;
}

int is_elif_header(const char *line) {
    return strncmp(line, "elif", 4) == 0 &&
           (line[4] == ' ' || line[4] == '\t' || line[4] == ':' || line[4] == '\0');
}

int is_else_header(const char *line) {
    return is_keyword_line(line, "else");
}

int is_while_header(const char *line) {
    return is_keyword_line(line, "while") || strncmp(line, "while ", 6) == 0;
}

int is_for_header(const char *line) {
    return is_keyword_line(line, "for") || strncmp(line, "for ", 4) == 0;
}

/* A function definition opens a block too, so the interpreter can skip it and
   script_find_end can count it when matching "end" lines. */
int is_fn_header(const char *line) {
    return strncmp(line, "fn", 2) == 0 &&
           (line[2] == ' ' || line[2] == '\t' || line[2] == '\0');
}

int is_end_line(const char *line) {
    return is_keyword_line(line, "end");
}

int is_block_opener(const char *line) {
    return is_if_header(line) || is_while_header(line) || is_for_header(line) ||
           is_fn_header(line);
}

/* ─────────────────────────────────────────────
   SCRIPT STORAGE
   ───────────────────────────────────────────── */

/* Source of Script.id. Every load gets a number of its own, so a function can
   say exactly which script it belongs to. */
static int next_script_id = 0;

/* Add one executable line to the script, growing the array when it is full. */
static void append_line(Script *script, const char *text, int line_num) {
    if (script->count == script->capacity) {
        int capacity = script->capacity == 0 ? 64 : script->capacity * 2;
        ScriptLine *grown = realloc(script->lines, (size_t)capacity * sizeof(*grown));
        if (!grown) {
            fprintf(stderr, COLOR_RED "script: out of memory" COLOR_RESET "\n");
            exit(EXIT_FAILURE);
        }
        script->lines    = grown;
        script->capacity = capacity;
    }

    ScriptLine *line = &script->lines[script->count++];
    line->text = strdup(text);
    if (!line->text) {
        fprintf(stderr, COLOR_RED "script: out of memory" COLOR_RESET "\n");
        exit(EXIT_FAILURE);
    }
    line->line_num = line_num;
    line->match    = 0;
}

/* Read a whole script into memory.
   Blank lines and comment lines are dropped, since they do nothing — that also
   means a comment like "# end" can never be mistaken for the end of a block.
   Returns 1 when the file could be read, or 0 when it could not be opened.
   Nothing is printed here: the caller knows whether a line number belongs in
   the error message. */
int script_load(const char *filename, Script *script) {
    script->lines         = NULL;
    script->count         = 0;
    script->capacity      = 0;
    script->last_line_num = 0;

    script->id      = ++next_script_id;
    script->running = 0;

    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    /* The same buffer size the streaming interpreter used, so an over-long
       line is still split into several pieces and line numbers stay the same
       as they always were. */
    char buffer[MAX_STR_LEN];
    while (fgets(buffer, sizeof(buffer), file)) {
        script->last_line_num++;

        char *trimmed = trim_whitespace(buffer);
        if (trimmed[0] != '\0' && trimmed[0] != '#') {
            append_line(script, trimmed, script->last_line_num);
        }
        free(trimmed);
    }

    fclose(file);
    return 1;
}

/* Index of the "end" that closes the block opened at opener, or -1 when the
   block never ends. Nested blocks are skipped in one step, and the answer is
   remembered so a loop body is only ever scanned once. */
int script_find_end(Script *script, int opener) {
    if (opener < 0 || opener >= script->count) return -1;
    if (script->lines[opener].match != 0) return script->lines[opener].match;

    int depth = 0;
    for (int i = opener + 1; i < script->count; i++) {
        const char *text = script->lines[i].text;

        if (is_block_opener(text)) {
            depth++;
        } else if (is_end_line(text)) {
            if (depth == 0) {
                script->lines[opener].match = i;
                return i;
            }
            depth--;
        }
    }

    script->lines[opener].match = -1;
    return -1;
}

/* Release everything script_load allocated. */
void script_free(Script *script) {
    for (int i = 0; i < script->count; i++) {
        free(script->lines[i].text);
    }
    free(script->lines);

    script->lines         = NULL;
    script->count         = 0;
    script->capacity      = 0;
    script->last_line_num = 0;
}