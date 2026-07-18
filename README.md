# newer

A small, robust command-line utility that synchronizes two files by copying the newer one over the older one.

## What it does

Given two files, `newer` compares their modification times and overwrites the older file with the contents of the newer file. It also replicates metadata (permissions, timestamps, and optionally owner/group) so the destination faithfully mirrors the source.

If one of the files does not exist, the existing file is copied to the missing path. If both are missing or both have identical timestamps, nothing happens.

## Features

- **Metadata preservation** — copies permissions, timestamps (with nanosecond precision via `futimens`), owner, and group.
- **Same-file safety** — detects if the two paths point to the same inode and aborts to avoid self-destruction.
- **Atomic writes** (`-a`) — writes to a temporary file in the destination directory and renames it into place, preventing half-written files on crash or power loss.
- **TOCTOU-safe** — uses file-descriptor-based calls (`fchmod`, `fchown`, `futimens`) instead of path-based calls whenever possible.
- **Dry-run mode** (`-n`) — preview what would happen without modifying any files.
- **Verbose mode** (`-v`) — print which file is being copied and why.
- **RAII resource management** — all file descriptors are managed automatically to prevent leaks.

## Build

```bash
make
```

Requires a C++17-compliant compiler and POSIX environment (Linux, macOS, etc.). No external dependencies.

## Usage

```bash
newer [options] <left> <right>
```

### Options

| Option            | Description                                                    |
|-------------------|----------------------------------------------------------------|
| `-v`, `--verbose` | Print detailed progress messages.                              |
| `-n`, `--dry-run` | Show what would happen without actually copying.               |
| `-a`, `--atomic`  | Perform an atomic write using a temporary file and `rename(2)`.|
| `-h`, `--help`    | Display help message.                                          |

### Examples

```bash
# Copy the newer of a.txt or b.txt to the other
newer a.txt b.txt

# Preview without changing anything
newer -n a.txt b.txt

# Atomic write with verbose output
newer -a -v a.txt b.txt
```

## Testing

Run the included shell test suite:

```bash
make test
```

Tests cover basic copying, permission synchronization, same-file detection, dry-run, verbose output, atomic mode, missing-file handling, and help display.

## Project Structure

```
newer/
├── src/
│   └── main.cpp      # Main source code
├── tests/
│   └── test.sh       # Shell-based test suite
├── Makefile          # Build configuration
└── qmake/
    └── newer.pro     # Qt Creator project file (console, no Qt dependency)
```

