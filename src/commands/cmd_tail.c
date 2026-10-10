#define _POSIX_C_SOURCE 200809L

#include "cmd_spec.h"
#include "argtable3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct tail_argtable {
    arg_lit_t *help;
    arg_int_t *lines;
    arg_file_t *files;
    arg_end_t *end;
    void *table[5];
} tail_argtable_t;

static int build_tail_argtable(tail_argtable_t *args)
{
    *args = (tail_argtable_t){0};

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
    fputs("Usage: tail ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void tail_print_usage(FILE *out)
{
    tail_argtable_t args;

    if (out == NULL) {
        return;
    }

    if (build_tail_argtable(&args) != 0) {
        fputs("tail: failed to allocate argument table\n", stderr);
        return;
    }

    print_usage_with_argtable(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

static char *duplicate_string(const char *input)
{
    size_t length;
    char *copy;

    if (input == NULL) {
        return NULL;
    }

    length = strlen(input);
    copy = malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, input, length + 1U);
    return copy;
}

static int print_tail_file(const char *path, int line_count)
{
    FILE *file;
    char **buffer = NULL;
    char *line = NULL;
    size_t buffer_capacity = 0;
    size_t line_capacity = 0;
    size_t stored = 0;
    size_t limit = (size_t)line_count;
    size_t next = 0;
    size_t i;
    ssize_t chars_read;
    int failed = 0;

    file = fopen(path, "r");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    if (limit == 0) {
        if (fclose(file) != 0) {
            perror(path);
            return 1;
        }
        return 0;
    }

    buffer_capacity = limit < 16 ? limit : 16;
    buffer = calloc(buffer_capacity, sizeof(*buffer));
    if (buffer == NULL) {
        fprintf(stderr, "tail: failed to allocate line buffer\n");
        fclose(file);
        return 1;
    }

    while ((chars_read = getline(&line, &line_capacity, file)) != -1) {
        char *copy = duplicate_string(line);

        if (copy == NULL) {
            fprintf(stderr, "tail: failed to allocate line storage\n");
            failed = 1;
            free(line);
            for (i = 0; i < stored; ++i) {
                free(buffer[i]);
            }
            free(buffer);
            fclose(file);
            return 1;
        }

        if (stored < limit) {
            if (stored == buffer_capacity) {
                size_t new_capacity = buffer_capacity > limit / 2
                                          ? limit
                                          : buffer_capacity * 2;

                if (new_capacity > (size_t)-1 / sizeof(*buffer)) {
                    fprintf(stderr, "tail: line buffer size is too large\n");
                    free(copy);
                    free(line);
                    for (i = 0; i < stored; ++i) {
                        free(buffer[i]);
                    }
                    free(buffer);
                    fclose(file);
                    return 1;
                }
                char **resized = realloc(buffer,
                                         new_capacity * sizeof(*buffer));

                if (resized == NULL) {
                    fprintf(stderr, "tail: failed to grow line buffer\n");
                    free(copy);
                    free(line);
                    for (i = 0; i < stored; ++i) {
                        free(buffer[i]);
                    }
                    free(buffer);
                    fclose(file);
                    return 1;
                }
                buffer = resized;
                buffer_capacity = new_capacity;
            }
            buffer[stored++] = copy;
        } else {
            free(buffer[next]);
            buffer[next] = copy;
            next = (next + 1) % limit;
        }
    }

    free(line);

    if (ferror(file)) {
        perror(path);
        failed = 1;
    }

    for (i = 0; i < stored; ++i) {
        size_t index = stored == limit ? (next + i) % stored : i;

        if (fputs(buffer[index], stdout) == EOF) {
            perror("tail: stdout");
            failed = 1;
            break;
        }
    }

    for (i = 0; i < stored; ++i) {
        free(buffer[i]);
    }
    free(buffer);

    if (fclose(file) != 0) {
        perror(path);
        failed = 1;
    }

    return failed ? 1 : 0;
}

int tail_run(int argc, char **argv)
{
    tail_argtable_t args;
    int nerrors;
    int status = 0;
    int i;
    int line_count;

    if (build_tail_argtable(&args) != 0) {
        fputs("tail: failed to allocate argument table\n", stderr);
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
        if (print_tail_file(args.files->filename[i], line_count) != 0) {
            status = 1;
            break;
        }
    }

    if (fflush(stdout) == EOF) {
        perror("tail: stdout");
        status = 1;
    }

    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_tail_spec = {
    "tail",
    "output the last part of files",
    "Print the last N lines of each FILE to standard output.",
    tail_run,
    tail_print_usage
};

void register_tail_command(void)
{
    register_command(&cmd_tail_spec);
}
