#define _POSIX_C_SOURCE 200809L

#include "argtable3.h"
#include "cmd_spec.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct rm_argtable {
    arg_lit_t *help;
    arg_lit_t *recursive;
    arg_lit_t *force;
    arg_file_t *files;
    arg_end_t *end;
    void *table[6];
} rm_argtable_t;

static int build_rm_argtable(rm_argtable_t *args)
{
    args->help = arg_litn("h", "help", 0, 1, "show usage and description");
    args->recursive = arg_litn("r", "recursive", 0, 1,
                               "remove directories and their contents");
    args->force = arg_litn("f", "force", 0, 1,
                           "ignore nonexistent files and arguments");
    args->files = arg_filen(NULL, NULL, "<file>...", 1, 100,
                            "file or directory path(s) to remove");
    args->end = arg_end(20);
    args->table[0] = args->help;
    args->table[1] = args->recursive;
    args->table[2] = args->force;
    args->table[3] = args->files;
    args->table[4] = args->end;
    args->table[5] = NULL;
    if (args->help == NULL || args->recursive == NULL || args->force == NULL ||
        args->files == NULL || args->end == NULL) {
        arg_freetable(args->table, sizeof(args->table) / sizeof(args->table[0]));
        return -1;
    }
    return 0;
}

static void print_rm_usage(FILE *out, void **table)
{
    fputs("Usage: rm ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void rm_print_usage(FILE *out)
{
    rm_argtable_t args;

    if (out == NULL) {
        return;
    }
    if (build_rm_argtable(&args) != 0) {
        fputs("rm: failed to allocate argument table\n", stderr);
        return;
    }
    print_rm_usage(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

static char *join_path(const char *directory, const char *name)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    int needs_slash = directory_length > 0 &&
                      directory[directory_length - 1] != '/';
    char *joined = malloc(directory_length + (size_t)needs_slash +
                          name_length + 1);

    if (joined == NULL) {
        return NULL;
    }
    memcpy(joined, directory, directory_length);
    if (needs_slash) {
        joined[directory_length++] = '/';
    }
    memcpy(joined + directory_length, name, name_length + 1);
    return joined;
}

static char *trim_trailing_slashes(const char *path)
{
    size_t length = strlen(path);
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, path, length + 1);
    while (length > 1 && copy[length - 1] == '/') {
        copy[--length] = '\0';
    }
    return copy;
}

static int remove_path(const char *path, int recursive, int force)
{
    struct stat st;

    if (lstat(path, &st) != 0) {
        if (force && errno == ENOENT) {
            return 0;
        }
        perror(path);
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(path) != 0) {
            if (force && errno == ENOENT) {
                return 0;
            }
            perror(path);
            return -1;
        }
        return 0;
    }
    if (!recursive) {
        fprintf(stderr, "rm: cannot remove '%s': is a directory\n", path);
        return -1;
    }

    {
        DIR *directory = opendir(path);
        struct dirent *entry;
        int status = 0;

        if (directory == NULL) {
            if (force && errno == ENOENT) {
                return 0;
            }
            perror(path);
            return -1;
        }
        errno = 0;
        while ((entry = readdir(directory)) != NULL) {
            char *child;

            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            child = join_path(path, entry->d_name);
            if (child == NULL) {
                fputs("rm: out of memory\n", stderr);
                status = -1;
                break;
            }
            if (remove_path(child, 1, force) != 0) {
                status = -1;
            }
            free(child);
            if (status != 0) {
                break;
            }
            errno = 0;
        }
        if (entry == NULL && errno != 0) {
            perror(path);
            status = -1;
        }
        if (closedir(directory) != 0) {
            perror(path);
            status = -1;
        }
        if (status != 0) {
            return -1;
        }
    }

    if (rmdir(path) != 0) {
        if (force && errno == ENOENT) {
            return 0;
        }
        perror(path);
        return -1;
    }
    return 0;
}

int rm_run(int argc, char **argv)
{
    rm_argtable_t args;
    int parse_errors;
    int status = 0;
    int i;

    if (build_rm_argtable(&args) != 0) {
        fputs("rm: failed to allocate argument table\n", stderr);
        return 1;
    }
    parse_errors = arg_parse(argc, argv, args.table);
    if (args.help->count > 0) {
        print_rm_usage(stdout, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 0;
    }
    if (parse_errors > 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        print_rm_usage(stderr, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    for (i = 0; i < args.files->count; ++i) {
        char *path = trim_trailing_slashes(args.files->filename[i]);

        if (path == NULL) {
            fputs("rm: out of memory\n", stderr);
            status = 1;
            continue;
        }
        if (remove_path(path, args.recursive->count > 0,
                        args.force->count > 0) != 0) {
            status = 1;
        }
        free(path);
    }
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_rm_spec = {
    "rm",
    "remove files and directories",
    "Remove FILE(s). Use -r to remove directory trees and -f to ignore "
    "nonexistent paths.",
    rm_run,
    rm_print_usage
};

void register_rm_command(void)
{
    register_command(&cmd_rm_spec);
}
