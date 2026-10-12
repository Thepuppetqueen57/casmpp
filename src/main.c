#include "casmpp.h"

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