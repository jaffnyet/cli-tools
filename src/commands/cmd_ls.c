#define _POSIX_C_SOURCE 200809L

#include "cmd_spec.h"
#include "argtable3.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

typedef struct ls_argtable {
    arg_lit_t *all;
    arg_lit_t *long_format;
    arg_lit_t *help;
    arg_file_t *directory;
    arg_end_t *end;
    void *table[6];
} ls_argtable_t;

typedef struct ls_entry {
    char *name;
} ls_entry_t;

static int build_ls_argtable(ls_argtable_t *args)
{
    *args = (ls_argtable_t){0};

    args->all = arg_lit0("a", "all", "list all entries, including . and ..");
    args->table[0] = args->all;
    args->long_format = arg_lit0("l", NULL, "use long listing format");
    args->table[1] = args->long_format;
    args->help = arg_lit0("h", "help", "show usage and description");
    args->table[2] = args->help;
    args->directory = arg_file0(NULL, NULL, "<dir>", "directory to list");
    args->table[3] = args->directory;
    args->end = arg_end(20);
    args->table[4] = args->end;
    args->table[5] = NULL;

    if (args->all == NULL || args->long_format == NULL || args->help == NULL ||
        args->directory == NULL || args->end == NULL) {
        arg_freetable(args->table, sizeof(args->table) / sizeof(args->table[0]));
        return -1;
    }

    return 0;
}

static void print_usage_with_argtable(FILE *out, void **table)
{
    fputs("Usage: ls ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void ls_print_usage(FILE *out)
{
    ls_argtable_t args;

    if (out == NULL) {
        return;
    }

    if (build_ls_argtable(&args) != 0) {
        fputs("ls: failed to allocate argument table\n", stderr);
        return;
    }

    print_usage_with_argtable(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

static int compare_entries(const void *left, const void *right)
{
    const ls_entry_t *entry_left = left;
    const ls_entry_t *entry_right = right;

    return strcmp(entry_left->name, entry_right->name);
}

static void format_permissions(mode_t mode, char permissions[11])
{
    permissions[0] = S_ISDIR(mode) ? 'd' :
                     S_ISLNK(mode) ? 'l' :
                     S_ISCHR(mode) ? 'c' :
                     S_ISBLK(mode) ? 'b' :
                     S_ISFIFO(mode) ? 'p' :
                     S_ISSOCK(mode) ? 's' : '-';
    permissions[1] = (mode & S_IRUSR) ? 'r' : '-';
    permissions[2] = (mode & S_IWUSR) ? 'w' : '-';
    permissions[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') :
                     ((mode & S_IXUSR) ? 'x' : '-');
    permissions[4] = (mode & S_IRGRP) ? 'r' : '-';
    permissions[5] = (mode & S_IWGRP) ? 'w' : '-';
    permissions[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') :
                     ((mode & S_IXGRP) ? 'x' : '-');
    permissions[7] = (mode & S_IROTH) ? 'r' : '-';
    permissions[8] = (mode & S_IWOTH) ? 'w' : '-';
#ifdef S_ISVTX
    permissions[9] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') :
                     ((mode & S_IXOTH) ? 'x' : '-');
#else
    permissions[9] = (mode & S_IXOTH) ? 'x' : '-';
#endif
    permissions[10] = '\0';
}

static int print_long_entry(DIR *directory, const char *name)
{
    struct stat info;
    struct passwd *owner;
    struct group *group;
    char permissions[11];
    char owner_id[32];
    char group_id[32];
    char timestamp[32];
    struct tm *modified;
    if (fstatat(dirfd(directory), name, &info, AT_SYMLINK_NOFOLLOW) != 0) {
        perror(name);
        return -1;
    }

    format_permissions(info.st_mode, permissions);

    owner = getpwuid(info.st_uid);
    if (owner == NULL) {
        (void)snprintf(owner_id, sizeof(owner_id), "%ju", (uintmax_t)info.st_uid);
    }

    group = getgrgid(info.st_gid);
    if (group == NULL) {
        (void)snprintf(group_id, sizeof(group_id), "%ju", (uintmax_t)info.st_gid);
    }

    modified = localtime(&info.st_mtime);
    if (modified == NULL ||
        strftime(timestamp, sizeof(timestamp), "%b %e %H:%M", modified) == 0) {
        (void)snprintf(timestamp, sizeof(timestamp), "??? ?? ??:??");
    }

    if (printf("%s %3ju %-8s %-8s %8ju %s %s\n",
               permissions,
               (uintmax_t)info.st_nlink,
               owner != NULL ? owner->pw_name : owner_id,
               group != NULL ? group->gr_name : group_id,
               (uintmax_t)info.st_size,
               timestamp,
               name) < 0) {
        perror("ls: stdout");
        return -1;
    }

    return 0;
}

static int append_entry(ls_entry_t **entries, size_t *count, size_t *capacity,
                        const char *name)
{
    ls_entry_t *grown;
    char *copy;
    size_t name_length = strlen(name) + 1;

    if (*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 16 : *capacity * 2;

        if (new_capacity < *capacity ||
            new_capacity > SIZE_MAX / sizeof(**entries)) {
            errno = ENOMEM;
            return -1;
        }

        grown = realloc(*entries, new_capacity * sizeof(**entries));
        if (grown == NULL) {
            return -1;
        }
        *entries = grown;
        *capacity = new_capacity;
    }

    copy = malloc(name_length);
    if (copy == NULL) {
        return -1;
    }
    memcpy(copy, name, name_length);

    (*entries)[*count].name = copy;
    ++*count;
    return 0;
}

static void free_entries(ls_entry_t *entries, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        free(entries[i].name);
    }
    free(entries);
}

int ls_run(int argc, char **argv)
{
    ls_argtable_t args;
    DIR *directory;
    struct dirent *entry;
    ls_entry_t *entries = NULL;
    size_t entry_count = 0;
    size_t entry_capacity = 0;
    size_t i;
    const char *directory_path;
    int status = 0;
    int nerrors;

    if (build_ls_argtable(&args) != 0) {
        fputs("ls: failed to allocate argument table\n", stderr);
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

    directory_path = args.directory->count > 0 ?
                     args.directory->filename[0] : ".";
    directory = opendir(directory_path);
    if (directory == NULL) {
        perror(directory_path);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    for (;;) {
        errno = 0;
        entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0) {
                perror("ls: readdir");
                status = 1;
            }
            break;
        }

        if (!args.all->count && entry->d_name[0] == '.') {
            continue;
        }

        if (append_entry(&entries, &entry_count, &entry_capacity,
                         entry->d_name) != 0) {
            perror("ls: allocate directory entry");
            status = 1;
            break;
        }
    }

    if (status == 0) {
        if (entry_count > 1) {
            qsort(entries, entry_count, sizeof(*entries), compare_entries);
        }
        for (i = 0; i < entry_count; ++i) {
            if (args.long_format->count) {
                if (print_long_entry(directory, entries[i].name) != 0) {
                    status = 1;
                    break;
                }
            } else if (printf("%s\n", entries[i].name) < 0) {
                perror("ls: stdout");
                status = 1;
                break;
            }

            if (ferror(stdout)) {
                perror("ls: stdout");
                status = 1;
                break;
            }
        }
    }

    if (closedir(directory) != 0) {
        perror("ls: closedir");
        status = 1;
    }

    free_entries(entries, entry_count);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    if (status == 0 && fflush(stdout) == EOF) {
        perror("ls: stdout");
        status = 1;
    }
    return status;
}

cmd_spec_t cmd_ls_spec = {
    "ls",
    "list directory contents",
    "List information about files and directories in standard POSIX format.",
    ls_run,
    ls_print_usage
};

void register_ls_command(void)
{
    register_command(&cmd_ls_spec);
}
