#define _POSIX_C_SOURCE 200809L

#include "cmd_spec.h"
#include "argtable3.h"

#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

typedef struct stat_argtable {
    arg_lit_t *help;
    arg_file_t *files;
    arg_end_t *end;
    void *table[4];
} stat_argtable_t;

static int build_stat_argtable(stat_argtable_t *args)
{
    *args = (stat_argtable_t){0};
    args->help = arg_lit0("h", "help", "show usage and description");
    args->table[0] = args->help;
    args->files = arg_filen(NULL, NULL, "<file>", 1, 100, "file(s) to inspect");
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

static void print_stat_usage(FILE *out, void **table)
{
    fputs("Usage: stat ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void stat_print_usage(FILE *out)
{
    stat_argtable_t args;

    if (out == NULL) {
        return;
    }
    if (build_stat_argtable(&args) != 0) {
        fputs("stat: failed to allocate argument table\n", stderr);
        return;
    }
    print_stat_usage(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
}

static void format_stat_permissions(mode_t mode, char permissions[11])
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

static int print_stat_entry(const char *path)
{
    struct stat info;
    struct passwd *owner;
    struct tm accessed;
    struct tm modified;
    struct tm changed;
    char permissions[11];
    char access_time[64];
    char modify_time[64];
    char change_time[64];
    char owner_id[32];
    const char *owner_name;

    if (stat(path, &info) != 0) {
        perror(path);
        return -1;
    }

    owner = getpwuid(info.st_uid);
    if (owner == NULL) {
        (void)snprintf(owner_id, sizeof(owner_id), "%ju", (uintmax_t)info.st_uid);
        owner_name = owner_id;
    } else {
        owner_name = owner->pw_name;
    }

    if (localtime_r(&info.st_atime, &accessed) == NULL ||
        strftime(access_time, sizeof(access_time), "%Y-%m-%d %H:%M:%S %z",
                 &accessed) == 0 ||
        localtime_r(&info.st_mtime, &modified) == NULL ||
        strftime(modify_time, sizeof(modify_time), "%Y-%m-%d %H:%M:%S %z",
                 &modified) == 0 ||
        localtime_r(&info.st_ctime, &changed) == NULL ||
        strftime(change_time, sizeof(change_time), "%Y-%m-%d %H:%M:%S %z",
                 &changed) == 0) {
        fprintf(stderr, "stat: could not format timestamps for %s\n", path);
        return -1;
    }

    format_stat_permissions(info.st_mode, permissions);
    if (printf("  File: %s\n"
               "  Size: %jd\n"
               "  Permissions: %04o (%s)\n"
               "  Owner: %s (uid %ju)\n"
               "  Access: %s\n"
               "  Modify: %s\n"
               "  Change: %s\n",
               path, (intmax_t)info.st_size, (unsigned int)(info.st_mode & 07777),
               permissions, owner_name, (uintmax_t)info.st_uid, access_time,
               modify_time, change_time) < 0) {
        perror("stat: stdout");
        return -1;
    }
    return 0;
}

int stat_run(int argc, char **argv)
{
    stat_argtable_t args;
    int nerrors;
    int status = 0;
    int i;

    if (build_stat_argtable(&args) != 0) {
        fputs("stat: failed to allocate argument table\n", stderr);
        return 1;
    }

    nerrors = arg_parse(argc, argv, args.table);
    if (args.help->count > 0) {
        print_stat_usage(stdout, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 0;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        print_stat_usage(stderr, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    for (i = 0; i < args.files->count; ++i) {
        if (print_stat_entry(args.files->filename[i]) != 0) {
            status = 1;
        }
    }
    if (fflush(stdout) == EOF) {
        perror("stat: stdout");
        status = 1;
    }
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_stat_spec = {
    "stat",
    "display file metadata",
    "Display the size, permissions, owner, and access, modification, and change times of FILE(s).",
    stat_run,
    stat_print_usage
};

void register_stat_command(void)
{
    register_command(&cmd_stat_spec);
}
