# Interactive Backup Management System

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![POSIX](https://img.shields.io/badge/POSIX-Compliance-blue?style=for-the-badge)

## Overview
An interactive, low-level backup management system built in C for Linux environments. It provides real-time directory synchronization, differential restoration, and parallel backup execution.

Instead of relying on external shell commands (like `system()`), this project interacts directly with the Linux kernel via POSIX APIs, demonstrating a multiprocess architecture, asynchronous I/O, and native kernel event monitoring.

## Core Features & Systems Engineering

* **Real-Time Event Monitoring (`inotify`)**: Leverages the Linux `inotify` API to actively monitor source directories. Any file mutations (creations, modifications, deletions) or symlink alterations are mirrored to target directories instantly.
* **Multiprocess Architecture (`fork` / `exec`)**: Each backup target is managed by an independent, isolated child process. This ensures that the interactive CLI remains completely non-blocking and highly responsive, preventing single-target I/O bottlenecks from lagging the entire system.
* **High-Performance Asynchronous I/O (`io_uring` / `aio`)**: The restoration process utilizes asynchronous API calls and file hashing to perform *differential restores*. Instead of blindly copying all data, it calculates hashes to identify and transfer only the files that have mutated, drastically reducing disk I/O and execution time.
* **Smart Symlink Resolution**: Implements advanced symlink handling using `realpath`. If a symlink points to an absolute path within the source tree, the system dynamically rewrites the link in the backup target to maintain internal referential integrity. External links are preserved untouched.
* **Robust POSIX Signal Handling**: Graceful termination is enforced via strict `SIGINT` and `SIGTERM` handlers. Upon exit, the daemon securely reaps all zombie child processes, frees allocated heap memory, and releases file descriptors to prevent resource leaks.
* **Bash-like Quote Parsing**: Natively handles complex file paths containing whitespaces and special characters through custom quote-parsing algorithms.

## Architecture

The system operates via a decoupled architecture consisting of a Main CLI Thread and multiple Worker Processes:

1. **CLI Dispatcher**: Parses user input, handles path validation, and prevents infinite cascade loops (e.g., backing up a directory inside itself).
2. **Inotify Watcher**: Subscribes to filesystem events and dispatches sync instructions.
3. **Worker Processes**: Execute the physical data duplication in parallel.

## Interactive CLI Commands

The engine runs as an interactive shell. Available commands:

* `add <source_path> <target_path_1> [target_path_N]...`
  Initializes a new backup. Creates target directories if they do not exist (must be empty if they do). Spawns subprocesses for each target and begins real-time `inotify` monitoring.
* `list`
  Displays a real-time summary of all active backup streams and their respective subprocess PIDs.
* `restore <source_path> <target_path>`
  Initiates a blocking, differential restoration. Cleans up orphaned files in the source, calculates hashes, and asynchronously restores missing or modified files from the target back to the source.
* `end <source_path> <target_path>`
  Terminates the specific backup subprocess and stops `inotify` tracking for that link. The backed-up data remains safely intact.
* `exit`
  Gracefully shuts down the system, terminating all associated background tasks and freeing resources.

## Build & Run

**Prerequisites:** GCC, Linux environment (kernel strictly supporting `inotify` and asynchronous I/O APIs).

```bash
# Compile the project
make

# Start the interactive engine
./backup-system


