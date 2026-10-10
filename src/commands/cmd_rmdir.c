#include "cmd_spec.h"
#include "argtable3.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct rmdir_argtable {
    arg_lit_t *help;
    arg_file_t *directories;
    arg_end_t *end;
    void *table[4];
} rmdir_argtable_t;

static int build_rmdir_argtable(rmdir_argtable_t *args)
{
    *args = (rmdir_argtable_t){0};
    args->help = arg_lit0("h", "help", "show usage and description");
    args->table[0] = args->help;
    args->directories = arg_filen(NULL, NULL, "<dir>", 1, 100, "empty directory(ies) to remove");
    args->table[1] = args->directories;
    args->end = arg_end(20);
    args->table[2] = args->end;
    args->table[3] = NULL;

    if (args->help == NULL || args->directories == NULL || args->end == NULL) {
        arg_freetable(args->table, sizeof(args->table) / sizeof(args->table[0]));
        return -1;
    }
    return 0;
}

static void print_rmdir_usage(FILE *out, void **table)
{
    fputs("Usage: rmdir ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void rmdir_print_usage(FILE *out)
{
    rmdir_argtable_t args;

    if (out == NULL) {
        return;
    }
    if (build_rmdir_argtable(&args) != 0) {
        fputs("rmdir: failed to allocate argument table\n", stderr);
        return;
    }
    print_rmdir_usage(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

int rmdir_run(int argc, char **argv)
{
    rmdir_argtable_t args;
    int nerrors;
    int status = 0;
    int i;

    if (build_rmdir_argtable(&args) != 0) {
        fputs("rmdir: failed to allocate argument table\n", stderr);
        return 1;
    }
    nerrors = arg_parse(argc, argv, args.table);
    if (args.help->count > 0) {
        print_rmdir_usage(stdout, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        print_rmdir_usage(stderr, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    for (i = 0; i < args.directories->count; ++i) {
        const char *path = args.directories->filename[i];

        if (rmdir(path) != 0) {
            perror(path);
            status = 1;
        }
    }
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_rmdir_spec = {
    "rmdir",
    "remove empty directories",
    "Remove empty DIR(s). Non-empty directories are not removed.",
    rmdir_run,
    rmdir_print_usage
};

void register_rmdir_command(void)
{
    register_command(&cmd_rmdir_spec);
}
