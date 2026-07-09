#pragma once
#include <signal.h>
#include <sys/wait.h>

int add_backup(char *source, char **targets, int target_count);
void list_backups();
int end_backup(char* source, char** targets, int target_count);
int restore_backup(char* source, char* target);
