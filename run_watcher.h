#pragma once

#define _XOPEN_SOURCE 700
#include <sys/inotify.h>
#include <unistd.h> 
#include <limits.h>
#include <sys/stat.h>  
#include <signal.h>  
#include <stdio.h> 
#include <string.h>  
#include <dirent.h>

typedef struct Watch {
    int wd;
    char path[PATH_MAX];
} Watch;

void run_watcher_loop(const char* src_root, const char* tgt_root);
void add_watch_recursive(int fd, const char* path, Watch* watches, int* watch_count);
void sync_create(const char* src, const char* tgt);
void sync_delete_recursive(const char* target);
