#define _POSIX_C_SOURCE 200809L

#include "cmd_spec.h"
#include "argtable3.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct touch_argtable {
    arg_lit_t *help;
    arg_file_t *files;
    arg_end_t *end;
    void *table[4];
} touch_argtable_t;

static int build_touch_argtable(touch_argtable_t *args)
{
    *args = (touch_argtable_t){0};
    args->help = arg_lit0("h", "help", "show usage and description");
    args->table[0] = args->help;
    args->files = arg_filen(NULL, NULL, "<file>", 1, 100, "file(s) to update or create");
    args->table[1] = args->files;
    args->end = arg_end(20);
    args->table[2] = args->end;
    args->table[3] = NULL;

    if (args->help == NULL || args->files == NULL || args->end == NULL) {
        arg_freetable(args->table, sizeof(args->table) / sizeof(args->table[0]));
        return -1;
    }
    return 0;
}

static void print_touch_usage(FILE *out, void **table)
{
    fputs("Usage: touch ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void touch_print_usage(FILE *out)
{
    touch_argtable_t args;

    if (out == NULL) {
        return;
    }
    if (build_touch_argtable(&args) != 0) {
        fputs("touch: failed to allocate argument table\n", stderr);
        return;
    }
    print_touch_usage(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

static int touch_file(const char *path)
{
    int descriptor;

    if (utimensat(AT_FDCWD, path, NULL, 0) == 0) {
        return 0;
    }
    if (errno != ENOENT) {
        return -1;
    }

    descriptor = open(path, O_WRONLY | O_CREAT | O_NONBLOCK, 0666);
    if (descriptor < 0) {
        return -1;
    }
    if (close(descriptor) != 0) {
        return -1;
    }
    return utimensat(AT_FDCWD, path, NULL, 0);
}

int touch_run(int argc, char **argv)
{
    touch_argtable_t args;
    int nerrors;
    int status = 0;
    int i;

    if (build_touch_argtable(&args) != 0) {
        fputs("touch: failed to allocate argument table\n", stderr);
        return 1;
    }
    nerrors = arg_parse(argc, argv, args.table);
    if (args.help->count > 0) {
        print_touch_usage(stdout, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        print_touch_usage(stderr, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    for (i = 0; i < args.files->count; ++i) {
        const char *path = args.files->filename[i];

        if (touch_file(path) != 0) {
            perror(path);
            status = 1;
        }
    }
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_touch_spec = {
    "touch",
    "create files or update timestamps",
    "Set the access and modification times of FILE(s) to the current time, creating missing files.",
    touch_run,
    touch_print_usage
};

void register_touch_command(void)
{
    register_command(&cmd_touch_spec);
}
