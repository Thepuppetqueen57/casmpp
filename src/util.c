#include "casmpp.h"

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