# AI Work Log

## 2026-10-09 - AiShell foundation, `cat`, build automation, and verification

### User prompt / intent
Build the core for a modular C shell: define a command registry API, implement and register a `cat` command, add a shell entry point that lists and dispatches commands, and provide a Makefile plus an automated verification script.

### Changes made and rationale
- Added `cmd_spec_t` and the registry API, backed by a fixed-capacity command registry with command registration, lookup, and iteration.
- Implemented the `cat` module using Argtable3 for `-h`/`--help` and positional file arguments. The command prints input files to standard output and reports parse, file, and output errors.
- Added `src/shell_main.c` to register and list commands, find `cat`, then exercise its help output through the registry.
- Added a Makefile to compile the project sources into `aishell`, along with `clean` and `all` targets.
- Added executable `tests/verify_1.sh` for build, shell-startup/help, and missing-file-argument checks.
- Fixed the Argtable argument table layout by putting `arg_end(20)` after the arguments and adding a trailing `NULL` pointer slot. This makes the table's explicit storage termination clear while retaining Argtable's `arg_end` parsing terminator.
- Replaced the shell entry point's hard-coded `cat -h` invocation with argument-based command lookup and dispatch. With no command, AiShell lists registered commands; unknown command tokens such as `-z` now report `aishell: command not found: -z` and return status 1.
- Extended the verifier to test actual `cat` file output, `cat -h`, and the requested unknown-command behavior.

### Debugging and verification
- Initial build attempts exposed missing Argtable support (`argtable3_private.h` and `xmalloc`/`xfree`) and later a missing `arg_litn` declaration in the dependency. These were addressed before the latest test run.
- An earlier run built successfully but `./aishell` segfaulted with status 139. The argument table was subsequently updated with the trailing `NULL` slot.
- Latest `./tests/verify_1.sh` run passed all stages:
  - Build: `make clean` and `make` produced `aishell`.
  - Happy path: `./aishell` exited with status 0 and printed the registered `cat` command and help text.
  - Error path: invoking `cat` without a required file produced the missing-argument error and returned status 1.
- After fixing command dispatch, the verifier now checks file output, help output, unknown commands, and missing required file arguments.

### How to verify
From the project root, run:

```sh
make clean
make
./tests/verify_1.sh
```

## 2026-10-09 - Add the `ls` command module

### User prompt / intent
Add a fully integrated `ls` command following the project's `cmd_spec_t`, Argtable3, POSIX directory APIs, and C99 standards.

### Changes made and rationale
- Added `src/commands/cmd_ls.c` with declarative `-a`, `-l`, and `-h` options, an optional directory argument defaulting to `.`, and an Argtable table ending with `arg_end(20)` plus a `NULL` sentinel.
- Implemented sorted directory output, filtering dot-prefixed entries by default, `-a` inclusion, and `-l` metadata output with file type/permissions, link count, owner/group, size, and modification time.
- Added allocation, directory-read, metadata, output, and close-error handling; Argtable allocations and collected entry names are released.
- Registered `ls` in `src/shell_main.c` and added its object to the Makefile build.
- Extended `tests/verify_1.sh` to check default filtering, `-a`, `-l`, help, and nonexistent-directory handling.
- Made sticky-bit display conditional because `S_ISVTX` is not exposed by the platform's strict C99/POSIX feature macros, keeping the module portable across this build configuration.
- Extended regression checks for default directory `.`, unknown `ls` options, and checked stdout flush failures.

### How to verify
Run `make clean && make` and `./tests/verify_1.sh` from the project root.

The latest verification run passed build, both existing `cat` checks, `ls` listing/flags/errors/help, the shell's unknown-command check, and the missing-argument check for `cat`.

## 2026-10-09 - Add top-level command help

### User prompt / intent
Make `help` and no-argument startup list available commands, and let `help <command>` show command documentation while keeping regular command dispatch unchanged.

### Changes made and rationale
- Updated `src/shell_main.c` to route no arguments and the top-level `help` keyword to command listing.
- Added command-specific help lookup, printing its summary, long description, and registered usage printer; unknown command names return status 1 with a clear error.
- Kept non-help command lookup and dispatch unchanged, and reject extra arguments to top-level help with a usage error.
- Added regression checks to `tests/verify_1.sh` for command listing, `help cat`, and unknown-command help.

### How to verify
Run `./tests/verify_1.sh` from the project root. It should confirm the listing, command-specific help output, and unknown-command error in addition to existing command checks.

## 2026-10-09 - Add file, metadata, and directory commands

### User prompt / intent
Implement and integrate `stat`, `head`, `tail`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`, and `touch` as complete command modules following AiShell's `cmd_spec_t`, Argtable3, C99, POSIX, and help/error conventions.

### Changes made and rationale
- Added a separate command module for each requested command, with command metadata, registration helpers, Argtable3 argument declarations, usage output, and `-h`/`--help` handling.
- Implemented metadata and timestamp reporting in `stat`; first/last-line output in `head` and `tail`; file and recursive directory operations for `cp`, `mv`, and `rm`; and directory/file creation and removal in `mkdir`, `rmdir`, and `touch`.
- Registered all nine commands in `src/shell_main.c` and added their translation units to the Makefile.
- Extended the bundled Argtable3 interface with integer options so `head` and `tail` parse `-n`/`--lines` declaratively instead of manually inspecting `argv`. The parser validates integer syntax and range; the commands reject negative counts.
- Improved Argtable constructor allocation handling for positional file arrays and parser error storage, avoiding partially initialized argument objects on allocation failure.
- Changed `tail` to grow its line pointer buffer only as needed and retain lines in a ring, rather than allocating based on the requested maximum up front. Zero-line requests still open the named input so missing files are reported.
- Extended `tests/verify_1.sh` to check command help, required operands, metadata, head/tail counts and invalid values, file and recursive directory operations, directory creation/removal, and timestamp-file creation.

### Debugging and verification
- Confirmed the `head`/`tail` `-n` implementation uses Argtable3, has no manual flag parsing, and rejects invalid and negative counts.
- Confirmed zero-line `head`/`tail` requests produce no output for valid files and do not suppress errors for nonexistent files.
- `make clean && make` completed successfully for the full project with the configured `-Wall -Wextra -std=c99` flags.
- `bash -n tests/verify_1.sh` passed.
- The full `./tests/verify_1.sh` run passed all seven stages, including build, command dispatch/help, all nine new commands, and their exercised happy/error paths.

### How to verify
From the project root, run:

```sh
make clean && make
bash -n tests/verify_1.sh
./tests/verify_1.sh
```
