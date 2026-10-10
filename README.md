# cli-tools

A modular, extensible command-line shell written in C99, implementing standard POSIX file and directory utilities with declarative argument parsing.

---

## Overview

`cli-tools` provides a unified command dispatch interface inspired by multi-call binaries like Git and BusyBox. Instead of using hardcoded branching, the shell is built on an interface abstraction and dynamic registry pattern, allowing commands to be defined independently and registered at runtime.

---

## Architecture Highlights

- **Registry-Based Command Dispatch**: Commands are decoupled from the main entry point via a `cmd_spec_t` contract, encapsulating command metadata, documentation, usage display, and execution callbacks.
- **Declarative Argument Parsing**: Leverages `argtable3` for robust POSIX-compliant option handling, type-safe validation, and auto-generated usage glossaries.
- **Memory Safety**: Strict parser table memory management, including proper sentinel termination to prevent segmentation faults during parsing and deallocation.
- **Extensible Anatomy**: Adding new commands requires zero modifications to the core dispatch engine—each command defines its own specification and registers on startup.

---

## Implemented Commands

| Command | Summary | Description |
| :--- | :--- | :--- |
| `cat` | Print file contents | Concatenate and display files to standard output |
| `stat` | Show file metadata | Display file size, permissions, owner, and timestamps |
| `head` | Output beginning of files | Print the first 10 lines (or custom with `-n`) |
| `tail` | Output end of files | Print the last 10 lines (or custom with `-n`) |
| `cp` | Copy files/directories | Copy file contents; supports recursive copy (`-r`) |
| `mv` | Move/rename files | Move or rename files and directories |
| `rm` | Remove files/directories | Delete files; supports recursive (`-r`) and force (`-f`) |
| `mkdir` | Create directories | Create directories; supports parent path creation (`-p`) |
| `rmdir` | Remove empty directories | Remove empty directories safely |
| `touch` | Update file timestamps | Update access/modification times or create empty file |

---

## Project Structure

```text
cli-tools/
├── include/
│   └── cmd_spec.h        # Command specification interface and registry API
├── src/
│   ├── commands/         # Individual command implementations (cmd_*.c)
│   ├── registry.c        # Dynamic command registry implementation
│   └── shell_main.c      # Shell entrypoint and command dispatcher
├── tests/                # Automated verification scripts
├── Makefile              # Build automation
└── README.md
