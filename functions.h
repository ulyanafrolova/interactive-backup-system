#pragma once

#define _XOPEN_SOURCE 700
#define MAX_TARGETS 1024
#define MAX_BACKUPS 1024
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <stdbool.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct {
    char* source;
    char* targets[MAX_TARGETS];
    pid_t pids[MAX_TARGETS];
    int target_count;
} Backup;

extern int backup_count;
extern Backup backups[MAX_BACKUPS];

// Add backup
char* copy_string(const char* src);
bool target_inside_source(const char* source, const char* target);
bool backup_already_exists(const char* source, const char* target);
int prepare_target(const char* source, const char* target, char* real_target);
bool directory_exists(const char* path);
bool is_directory_empty(const char* path);
int copy_file(const char* source, const char* destination);
int handle_symlink(const char* src, const char* tgt, const char* src_root, const char* tgt_root);
int copy_tree(const char* src, const char* dst, const char* src_root, const char* dst_root);

// Restore backup
bool files_differ(const char* src, const char* dst);
int restore_copy(const char* tgt, const char* src, const char* tgt_root, const char* src_root);
int restore_remove(const char* src, const char* tgt_root, const char* src_root);
