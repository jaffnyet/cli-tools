#include "cmd_spec.h"
#include "argtable3.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct cat_argtable {
    arg_lit_t *help;
    arg_file_t *files;
    arg_end_t *end;
    void *table[4];
} cat_argtable_t;

static int build_cat_argtable(cat_argtable_t *args)
{
    /* Keep the terminator last so Argtable3 can safely parse and free the table. */
    args->help = arg_litn("h", "help", 0, 1, "show usage and description");
    args->files = arg_filen(NULL, NULL, "<file>", 1, 100, "file(s) to print");
    args->end = arg_end(20);
    args->table[0] = args->help;
    args->table[1] = args->files;
    args->table[2] = args->end;
    args->table[3] = NULL;

    if (args->help == NULL || args->files == NULL || args->end == NULL) {
        arg_freetable(args->table, sizeof(args->table) / sizeof(args->table[0]));
        return -1;
    }

    return 0;
}

static void print_usage_with_argtable(FILE *out, void **table)
{
    fputs("Usage: cat ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void cat_print_usage(FILE *out)
{
    cat_argtable_t args;

    if (out == NULL) {
        return;
    }

    if (build_cat_argtable(&args) != 0) {
        fputs("cat: failed to allocate argument table\n", stderr);
        return;
    }

    print_usage_with_argtable(out, args.table);

    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

int cat_run(int argc, char **argv)
{
    cat_argtable_t args;
    int nerrors;
    int status = 0;
    int i;
    char buffer[4096];

    if (build_cat_argtable(&args) != 0) {
        fputs("cat: failed to allocate argument table\n", stderr);
        return 1;
    }

    nerrors = arg_parse(argc, argv, args.table);
    if (args.help->count > 0) {
        print_usage_with_argtable(stdout, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        print_usage_with_argtable(stderr, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    for (i = 0; i < args.files->count; ++i) {
        FILE *file = fopen(args.files->filename[i], "r");

        if (file == NULL) {
            perror(args.files->filename[i]);
            status = 1;
            break;
        }

        /* Copy in chunks to handle files larger than the input buffer. */
        for (;;) {
            size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);

            if (bytes_read > 0 && fwrite(buffer, 1, bytes_read, stdout) != bytes_read) {
                perror("cat: stdout");
                status = 1;
                break;
            }

            if (bytes_read < sizeof(buffer)) {
                if (ferror(file)) {
                    perror(args.files->filename[i]);
                    status = 1;
                }
                break;
            }
        }

        if (fclose(file) != 0) {
            perror(args.files->filename[i]);
            status = 1;
        }

        if (status != 0) {
            break;
        }
    }

    if (status == 0 && fflush(stdout) == EOF) {
        perror("cat: stdout");
        status = 1;
    }

    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_cat_spec = {
    "cat",
    "print file contents",
    "Concatenate FILE(s) to standard output.",
    cat_run,
    cat_print_usage
};

void register_cat_command(void)
{
    register_command(&cmd_cat_spec);
}
