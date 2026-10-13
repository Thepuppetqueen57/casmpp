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

/* ─────────────────────────────────────────────
   PATH UTILITY
   ───────────────────────────────────────────── */

/* Copy the directory part of a path into out, keeping the trailing separator
   ("" when the path has no directory at all). Both / and \ count as separators,
   so Windows paths and MSYS/Git Bash paths behave the same. */
void path_directory(const char *path, char *out, size_t out_size) {
    int last_separator = -1;

    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' || path[i] == '\\') last_separator = i;
    }

    if (last_separator < 0) {
        out[0] = '\0';
        return;
    }

    size_t length = (size_t)last_separator + 1;
    if (length >= out_size) length = out_size - 1;

    memcpy(out, path, length);
    out[length] = '\0';
}

/* Join a directory and a relative path into out.
   An empty directory leaves the path untouched, and a separator is only added
   when the directory does not already end with one. */
void join_path(const char *dir, const char *relative, char *out, size_t out_size) {
    if (dir[0] == '\0') {
        snprintf(out, out_size, "%s", relative);
        return;
    }

    size_t length = strlen(dir);
    if (dir[length - 1] == '/' || dir[length - 1] == '\\') {
        snprintf(out, out_size, "%s%s", dir, relative);
    } else {
        snprintf(out, out_size, "%s/%s", dir, relative);
    }
}