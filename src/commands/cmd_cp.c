#define _POSIX_C_SOURCE 200809L

#include "argtable3.h"
#include "cmd_spec.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct cp_argtable {
    arg_lit_t *help;
    arg_lit_t *recursive;
    arg_file_t *paths;
    arg_end_t *end;
    void *table[5];
} cp_argtable_t;

static int build_cp_argtable(cp_argtable_t *args)
{
    args->help = arg_litn("h", "help", 0, 1, "show usage and description");
    args->recursive = arg_litn("r", "recursive", 0, 1,
                               "copy directories recursively");
    args->paths = arg_filen(NULL, NULL, "<source>... <destination>", 2, 100,
                            "source path(s) and destination");
    args->end = arg_end(20);
    args->table[0] = args->help;
    args->table[1] = args->recursive;
    args->table[2] = args->paths;
    args->table[3] = args->end;
    args->table[4] = NULL;

    if (args->help == NULL || args->recursive == NULL || args->paths == NULL ||
        args->end == NULL) {
        arg_freetable(args->table, sizeof(args->table) / sizeof(args->table[0]));
        return -1;
    }
    return 0;
}

static void print_cp_usage(FILE *out, void **table)
{
    fputs("Usage: cp ", out);
    arg_print_syntax(out, table, "\n");
    fputs("Options:\n", out);
    arg_print_glossary(out, table, "  %-25s %s\n");
}

void cp_print_usage(FILE *out)
{
    cp_argtable_t args;

    if (out == NULL) {
        return;
    }
    if (build_cp_argtable(&args) != 0) {
        fputs("cp: failed to allocate argument table\n", stderr);
        return;
    }
    print_cp_usage(out, args.table);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
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

static char *path_basename(const char *path)
{
    size_t length = strlen(path);
    size_t start;
    char *base;

    while (length > 1 && path[length - 1] == '/') {
        --length;
    }
    if (length == 1 && path[0] == '/') {
        return strdup("/");
    }
    start = length;
    while (start > 0 && path[start - 1] != '/') {
        --start;
    }
    base = malloc(length - start + 1);
    if (base != NULL) {
        memcpy(base, path + start, length - start);
        base[length - start] = '\0';
    }
    return base;
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

static int same_file(const char *destination, const struct stat *source_stat)
{
    struct stat destination_stat;

    if (lstat(destination, &destination_stat) != 0) {
        return 0;
    }
    return source_stat->st_dev == destination_stat.st_dev &&
           source_stat->st_ino == destination_stat.st_ino;
}

static int target_is_within_source(const char *target,
                                  const struct stat *source_stat)
{
    char *candidate = strdup(target);

    if (candidate == NULL) {
        errno = ENOMEM;
        return -1;
    }
    for (;;) {
        struct stat candidate_stat;
        char *slash;

        if (stat(candidate, &candidate_stat) == 0 &&
            candidate_stat.st_dev == source_stat->st_dev &&
            candidate_stat.st_ino == source_stat->st_ino) {
            free(candidate);
            return 1;
        }
        slash = strrchr(candidate, '/');
        if (slash == NULL) {
            free(candidate);
            return 0;
        }
        if (slash == candidate) {
            if (candidate[1] == '\0') {
                free(candidate);
                return 0;
            }
            candidate[1] = '\0';
        } else {
            *slash = '\0';
        }
    }
}

static int copy_symlink(const char *source, const char *destination,
                        const struct stat *source_stat)
{
    size_t capacity = source_stat->st_size > 0
                          ? (size_t)source_stat->st_size + 1
                          : 256;
    char *link_target = NULL;
    ssize_t length;
    struct stat destination_stat;

    for (;;) {
        char *resized = realloc(link_target, capacity);
        if (resized == NULL) {
            free(link_target);
            errno = ENOMEM;
            perror("cp");
            return -1;
        }
        link_target = resized;
        length = readlink(source, link_target, capacity);
        if (length < 0) {
            perror(source);
            free(link_target);
            return -1;
        }
        if ((size_t)length < capacity) {
            break;
        }
        if (capacity > (size_t)-1 / 2) {
            free(link_target);
            errno = ENAMETOOLONG;
            perror(source);
            return -1;
        }
        capacity *= 2;
    }
    link_target[length] = '\0';

    if (lstat(destination, &destination_stat) == 0) {
        if (source_stat->st_dev == destination_stat.st_dev &&
            source_stat->st_ino == destination_stat.st_ino) {
            fprintf(stderr, "cp: '%s' and '%s' are the same file\n",
                    source, destination);
            free(link_target);
            return -1;
        }
        if (S_ISDIR(destination_stat.st_mode) || unlink(destination) != 0) {
            if (S_ISDIR(destination_stat.st_mode)) {
                errno = EISDIR;
            }
            perror(destination);
            free(link_target);
            return -1;
        }
    } else if (errno != ENOENT) {
        perror(destination);
        free(link_target);
        return -1;
    }

    if (symlink(link_target, destination) != 0) {
        perror(destination);
        free(link_target);
        return -1;
    }
    free(link_target);
    return 0;
}

static int copy_regular_file(const char *source, const char *destination,
                             const struct stat *source_stat)
{
    int input = -1;
    int output = -1;
    char buffer[16384];
    int status = -1;
    struct stat destination_stat;

    if (same_file(destination, source_stat)) {
        fprintf(stderr, "cp: '%s' and '%s' are the same file\n",
                source, destination);
        return -1;
    }
    if (lstat(destination, &destination_stat) == 0) {
        if (S_ISDIR(destination_stat.st_mode)) {
            errno = EISDIR;
            perror(destination);
            return -1;
        }
        if (S_ISLNK(destination_stat.st_mode) && unlink(destination) != 0) {
            perror(destination);
            return -1;
        }
    } else if (errno != ENOENT) {
        perror(destination);
        return -1;
    }

    input = open(source, O_RDONLY);
    if (input < 0) {
        perror(source);
        return -1;
    }
    output = open(destination, O_WRONLY | O_CREAT | O_TRUNC,
                  source_stat->st_mode & 07777);
    if (output < 0) {
        perror(destination);
        goto cleanup;
    }

    for (;;) {
        ssize_t bytes_read = read(input, buffer, sizeof(buffer));
        size_t written = 0;

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror(source);
            goto cleanup;
        }
        if (bytes_read == 0) {
            break;
        }
        while (written < (size_t)bytes_read) {
            ssize_t bytes_written = write(output, buffer + written,
                                          (size_t)bytes_read - written);
            if (bytes_written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror(destination);
                goto cleanup;
            }
            if (bytes_written == 0) {
                errno = EIO;
                perror(destination);
                goto cleanup;
            }
            written += (size_t)bytes_written;
        }
    }
    if (fchmod(output, source_stat->st_mode & 07777) != 0) {
        perror(destination);
        goto cleanup;
    }
    status = 0;

cleanup:
    if (input >= 0 && close(input) != 0) {
        perror(source);
        status = -1;
    }
    if (output >= 0 && close(output) != 0) {
        perror(destination);
        status = -1;
    }
    return status;
}

static int copy_node(const char *source, const char *destination, int recursive);

static int copy_directory(const char *source, const char *destination,
                          const struct stat *source_stat)
{
    DIR *directory;
    struct dirent *entry;
    struct stat destination_stat;
    int created = 0;
    int status = 0;
    int within_source;

    if (same_file(destination, source_stat)) {
        fprintf(stderr, "cp: '%s' and '%s' are the same directory\n",
                source, destination);
        return -1;
    }
    within_source = target_is_within_source(destination, source_stat);
    if (within_source < 0) {
        perror("cp");
        return -1;
    }
    if (within_source) {
        fprintf(stderr, "cp: cannot copy '%s' into itself, '%s'\n",
                source, destination);
        return -1;
    }

    if (lstat(destination, &destination_stat) == 0) {
        if (!S_ISDIR(destination_stat.st_mode)) {
            errno = ENOTDIR;
            perror(destination);
            return -1;
        }
    } else if (errno == ENOENT) {
        if (mkdir(destination, source_stat->st_mode & 07777) != 0) {
            perror(destination);
            return -1;
        }
        created = 1;
    } else {
        perror(destination);
        return -1;
    }

    directory = opendir(source);
    if (directory == NULL) {
        perror(source);
        if (created) {
            (void)rmdir(destination);
        }
        return -1;
    }

    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        char *source_child;
        char *destination_child;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        source_child = join_path(source, entry->d_name);
        destination_child = join_path(destination, entry->d_name);
        if (source_child == NULL || destination_child == NULL) {
            fprintf(stderr, "cp: out of memory\n");
            free(source_child);
            free(destination_child);
            status = -1;
            break;
        }
        if (copy_node(source_child, destination_child, 1) != 0) {
            status = -1;
        }
        free(source_child);
        free(destination_child);
        if (status != 0) {
            break;
        }
        errno = 0;
    }
    if (entry == NULL && errno != 0) {
        perror(source);
        status = -1;
    }
    if (closedir(directory) != 0) {
        perror(source);
        status = -1;
    }
    if (status == 0 && created &&
        chmod(destination, source_stat->st_mode & 07777) != 0) {
        perror(destination);
        status = -1;
    }
    return status;
}

static int copy_node(const char *source, const char *destination, int recursive)
{
    struct stat source_stat;

    if (lstat(source, &source_stat) != 0) {
        perror(source);
        return -1;
    }
    if (S_ISLNK(source_stat.st_mode)) {
        return copy_symlink(source, destination, &source_stat);
    }
    if (S_ISDIR(source_stat.st_mode)) {
        if (!recursive) {
            fprintf(stderr, "cp: -r not specified; omitting directory '%s'\n",
                    source);
            return -1;
        }
        return copy_directory(source, destination, &source_stat);
    }
    if (S_ISREG(source_stat.st_mode)) {
        return copy_regular_file(source, destination, &source_stat);
    }
    fprintf(stderr, "cp: unsupported file type: '%s'\n", source);
    return -1;
}

int cp_run(int argc, char **argv)
{
    cp_argtable_t args;
    int parse_errors;
    int status = 0;
    int source_count;
    int destination_is_directory;
    struct stat destination_stat;
    char *destination_path;
    int i;

    if (build_cp_argtable(&args) != 0) {
        fputs("cp: failed to allocate argument table\n", stderr);
        return 1;
    }
    parse_errors = arg_parse(argc, argv, args.table);
    if (args.help->count > 0) {
        print_cp_usage(stdout, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 0;
    }
    if (parse_errors > 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        print_cp_usage(stderr, args.table);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    source_count = args.paths->count - 1;
    destination_path = trim_trailing_slashes(
        args.paths->filename[source_count]);
    if (destination_path == NULL) {
        fputs("cp: out of memory\n", stderr);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }
    destination_is_directory =
        stat(destination_path, &destination_stat) == 0 &&
        S_ISDIR(destination_stat.st_mode);
    if (source_count > 1 && !destination_is_directory) {
        fprintf(stderr, "cp: target '%s' is not a directory\n",
                destination_path);
        free(destination_path);
        arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
        return 1;
    }

    for (i = 0; i < source_count; ++i) {
        char *source = trim_trailing_slashes(args.paths->filename[i]);
        char *target = NULL;

        if (source == NULL) {
            fputs("cp: out of memory\n", stderr);
            status = 1;
            break;
        }
        if (destination_is_directory) {
            char *base = path_basename(source);
            if (base == NULL) {
                fputs("cp: out of memory\n", stderr);
                free(source);
                status = 1;
                break;
            }
            target = join_path(destination_path, base);
            free(base);
        } else {
            target = strdup(destination_path);
        }
        if (target == NULL) {
            fputs("cp: out of memory\n", stderr);
            free(source);
            status = 1;
            break;
        }
        if (copy_node(source, target, args.recursive->count > 0) != 0) {
            status = 1;
        }
        free(source);
        free(target);
        if (status != 0) {
            break;
        }
    }

    free(destination_path);
    arg_freetable(args.table, sizeof(args.table) / sizeof(args.table[0]));
    return status;
}

cmd_spec_t cmd_cp_spec = {
    "cp",
    "copy files and directories",
    "Copy SOURCE(s) to DESTINATION. Use -r to copy directories recursively.",
    cp_run,
    cp_print_usage
};

void register_cp_command(void)
{
    register_command(&cmd_cp_spec);
}
