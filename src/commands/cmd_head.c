#define _POSIX_C_SOURCE 200809L

#include "cmd_spec.h"
#include "argtable3.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct head_argtable {
    arg_lit_t *help;
    arg_int_t *lines;
    arg_file_t *files;
    arg_end_t *end;
    void *table[5];
} head_argtable_t;

static int build_head_argtable(head_argtable_t *args)
{
    *args = (head_argtable_t){0};

    args->help = arg_litn("h", "help", 0, 1, "show usage and description");
    args->lines = arg_int0("n", "lines", "<n>", "number of lines to print");
    args->files = arg_filen(NULL, NULL, "<file>", 1, 100, "file(s) to print");
    args->end = arg_end(20);
    args->table[0] = args->help;
    args->table[1] = args->lines;
    args->table[2] = args->files;
    args->table[3] = args->end;
    args->table[4] = NULL;

    if (args->help == NULL || args->lines == NULL || args->files == NULL || args->end == NULL) {
        arg_freetable(args->table, sizeof(args->table) / sizeof(args->table[0]));
        return -1;
    }

    args->lines->ival[0] = 10;
    return 0;
}

static void print_usage_with_argtable(FILE *out, void **table)
{
    fputs("Usage: head ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void head_print_usage(FILE *out)
{
    head_argtable_t args;

    if (out == NULL) {
        return;
    }

    if (build_head_argtable(&args) != 0) {
        fputs("head: failed to allocate argument table\n", stderr);
        return;
    }

    print_usage_with_argtable(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

static int print_head_file(const char *path, int line_count)
{
    FILE *file;
    char *line = NULL;
    size_t capacity = 0;
    ssize_t chars_read;
    int printed = 0;
    int failed = 0;

    file = fopen(path, "r");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    while (printed < line_count && (chars_read = getline(&line, &capacity, file)) != -1) {
        if (fwrite(line, 1, (size_t)chars_read, stdout) != (size_t)chars_read) {
            perror("head: stdout");
            failed = 1;
            break;
        }
        ++printed;
    }

    if (ferror(file)) {
        perror(path);
        failed = 1;
    }

    free(line);

    if (fclose(file) != 0) {
        perror(path);
        failed = 1;
    }

    return failed ? 1 : 0;
}

int head_run(int argc, char **argv)
{
    head_argtable_t args;
    int nerrors;
    int status = 0;
    int i;
    int line_count;

    if (build_head_argtable(&args) != 0) {
        fputs("head: failed to allocate argument table\n", stderr);
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

    if (args.lines->count > 0 && args.lines->ival[0] < 0) {
        fprintf(stderr, "%s: invalid line count: '%d'\n", argv[0], args.lines->ival[0]);
        print_usage_with_argtable(stderr, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    line_count = (args.lines->count > 0) ? args.lines->ival[0] : 10;
    for (i = 0; i < args.files->count; ++i) {
        if (print_head_file(args.files->filename[i], line_count) != 0) {
            status = 1;
            break;
        }
    }

    if (fflush(stdout) == EOF) {
        perror("head: stdout");
        status = 1;
    }

    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_head_spec = {
    "head",
    "output the first part of files",
    "Print the first N lines of each FILE to standard output.",
    head_run,
    head_print_usage
};

void register_head_command(void)
{
    register_command(&cmd_head_spec);
}
