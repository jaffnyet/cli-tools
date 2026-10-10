#define _POSIX_C_SOURCE 200809L

#include "cmd_spec.h"
#include "argtable3.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct mkdir_argtable {
    arg_lit_t *parents;
    arg_lit_t *help;
    arg_file_t *directories;
    arg_end_t *end;
    void *table[5];
} mkdir_argtable_t;

static int build_mkdir_argtable(mkdir_argtable_t *args)
{
    *args = (mkdir_argtable_t){0};
    args->parents = arg_lit0("p", NULL, "create parent directories as needed");
    args->table[0] = args->parents;
    args->help = arg_lit0("h", "help", "show usage and description");
    args->table[1] = args->help;
    args->directories = arg_filen(NULL, NULL, "<dir>", 1, 100, "directory(ies) to create");
    args->table[2] = args->directories;
    args->end = arg_end(20);
    args->table[3] = args->end;
    args->table[4] = NULL;

    if (args->parents == NULL || args->help == NULL ||
        args->directories == NULL || args->end == NULL) {
        arg_freetable(args->table, sizeof(args->table) / sizeof(args->table[0]));
        return -1;
    }
    return 0;
}

static void print_mkdir_usage(FILE *out, void **table)
{
    fputs("Usage: mkdir ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void mkdir_print_usage(FILE *out)
{
    mkdir_argtable_t args;

    if (out == NULL) {
        return;
    }
    if (build_mkdir_argtable(&args) != 0) {
        fputs("mkdir: failed to allocate argument table\n", stderr);
        return;
    }
    print_mkdir_usage(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

static int make_parents(const char *path)
{
    char *prefix;
    size_t length = strlen(path);
    size_t i;

    if (length == 0) {
        errno = ENOENT;
        return -1;
    }
    prefix = malloc(length + 1);
    if (prefix == NULL) {
        return -1;
    }
    memcpy(prefix, path, length + 1);

    for (i = 0; i <= length; ++i) {
        if (prefix[i] == '/' || prefix[i] == '\0') {
            char separator = prefix[i];

            prefix[i] = '\0';
            if (prefix[0] != '\0' && mkdir(prefix, 0777) != 0) {
                struct stat info;

                if (errno != EEXIST || stat(prefix, &info) != 0 ||
                    !S_ISDIR(info.st_mode)) {
                    int saved_errno = errno;
                    free(prefix);
                    errno = saved_errno;
                    return -1;
                }
            }
            prefix[i] = separator;
        }
    }
    free(prefix);
    return 0;
}

int mkdir_run(int argc, char **argv)
{
    mkdir_argtable_t args;
    int nerrors;
    int status = 0;
    int i;

    if (build_mkdir_argtable(&args) != 0) {
        fputs("mkdir: failed to allocate argument table\n", stderr);
        return 1;
    }
    nerrors = arg_parse(argc, argv, args.table);
    if (args.help->count > 0) {
        print_mkdir_usage(stdout, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        print_mkdir_usage(stderr, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    for (i = 0; i < args.directories->count; ++i) {
        const char *path = args.directories->filename[i];
        int result = args.parents->count > 0 ? make_parents(path) : mkdir(path, 0777);

        if (result != 0) {
            perror(path);
            status = 1;
        }
    }
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_mkdir_spec = {
    "mkdir",
    "create directories",
    "Create DIR(s). With -p, create missing parent directories and succeed when a requested directory already exists.",
    mkdir_run,
    mkdir_print_usage
};

void register_mkdir_command(void)
{
    register_command(&cmd_mkdir_spec);
}
