#include "cmd_spec.h"

#include <stdio.h>
#include <string.h>

/* Forward declaration of module registration */
extern void register_cat_command(void);
extern void register_ls_command(void);
extern void register_stat_command(void);
extern void register_head_command(void);
extern void register_tail_command(void);
extern void register_cp_command(void);
extern void register_mv_command(void);
extern void register_rm_command(void);
extern void register_mkdir_command(void);
extern void register_rmdir_command(void);
extern void register_touch_command(void);

/* Helper callback to list registered commands */
static void list_command_cb(const cmd_spec_t *spec, void *userdata)
{
    FILE *out = userdata;

    if (spec == NULL || out == NULL) {
        return;
    }

    fprintf(out, "  %-10s - %s\n",
            spec->name != NULL ? spec->name : "",
            spec->summary != NULL ? spec->summary : "");
}

int main(int argc, char **argv)
{
    const cmd_spec_t *cmd;

    register_cat_command();
    register_ls_command();
    register_stat_command();
    register_head_command();
    register_tail_command();
    register_cp_command();
    register_mv_command();
    register_rm_command();
    register_mkdir_command();
    register_rmdir_command();
    register_touch_command();

    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        if (argc > 3) {
            fputs("Usage: aishell help [command]\n", stderr);
            return 1;
        }

        if (argc == 3) {
            cmd = find_command(argv[2]);
            if (cmd == NULL) {
                fprintf(stderr, "aishell: command not found: %s\n", argv[2]);
                return 1;
            }

            printf("%s - %s\n", cmd->name, cmd->summary);
            if (cmd->long_help != NULL && cmd->long_help[0] != '\0') {
                printf("%s\n", cmd->long_help);
            }
            if (cmd->print_usage != NULL) {
                cmd->print_usage(stdout);
            }
            return 0;
        }

        puts("AiShell initialized.");
        puts("Available commands:");
        for_each_command(list_command_cb, stdout);
        return 0;
    }

    cmd = find_command(argv[1]);
    if (cmd == NULL) {
        fprintf(stderr, "aishell: command not found: %s\n", argv[1]);
        return 1;
    }
    if (cmd->run == NULL) {
        fprintf(stderr, "aishell: command cannot be run: %s\n", argv[1]);
        return 1;
    }

    return cmd->run(argc - 1, argv + 1);
}
