CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -std=c99

SOURCES = \
	src/shell_main.c \
	src/registry.c \
	src/argtable3.c \
	src/commands/cmd_cat.c \
	src/commands/cmd_ls.c \
	src/commands/cmd_stat.c \
	src/commands/cmd_head.c \
	src/commands/cmd_tail.c \
	src/commands/cmd_cp.c \
	src/commands/cmd_mv.c \
	src/commands/cmd_rm.c \
	src/commands/cmd_mkdir.c \
	src/commands/cmd_rmdir.c \
	src/commands/cmd_touch.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: aishell

aishell: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) aishell \
		test.txt \
		tests/test_cat_error.c \
		tests/test_cat_error \
		tests/test_cat_error.log \
		tests/aishell_test.log
