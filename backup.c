#define _XOPEN_SOURCE 700

#include "backup.h"
#include "functions.h"
#include "run_watcher.h"

// Adds a new backup
int add_backup(char *source, char **targets, int target_count) {
    char real_src[PATH_MAX];
    if (!realpath(source, real_src)) ERR("realpath source");

    int b = backup_count++;
    backups[b].source = copy_string(real_src);
    backups[b].target_count = 0;

    for (int i = 0; i < target_count; i++) {
        char real_tgt[PATH_MAX];
        int prepared_target = prepare_target(real_src, targets[i], real_tgt);
        if (prepared_target != 0) continue;
        if (directory_exists(real_tgt)) {
            if (!is_directory_empty(real_tgt)) {
                fprintf(stderr, "Target directory exists and is not empty: %s\n", real_tgt);
                continue;
            }
        }
        else {
            if (mkdir(real_tgt, 0777) != 0) {
                perror("mkdir");
                continue;
            }
        }
        if (copy_tree(real_src, real_tgt, real_src, real_tgt) != EXIT_SUCCESS) ERR("copy_tree");
        pid_t pid = fork();
        if (pid < 0) ERR("fork");
        if (pid == 0) {
            run_watcher_loop(real_src, real_tgt); 
            exit(EXIT_SUCCESS);
        }
        int k = backups[b].target_count++;
        backups[b].targets[k] = copy_string(real_tgt);
        backups[b].pids[k] = pid;
        printf("Backup created: %s -> %s\n", real_src, real_tgt);
    }
    if (backups[b].target_count == 0) {
        free(backups[b].source);
        backup_count--;
    }
    return EXIT_SUCCESS;
}

// Lists all the backups with targets and pids
void list_backups() {
    if (backup_count == 0) {
        printf("No backups.\n");
        return;
    }
    for (int i = 0; i < backup_count; i++) {
        printf("Backup %d:\n", i+1);
        printf("   Source:  %s\n", backups[i].source);
        for (int j = 0; j < backups[i].target_count; j++) {
            printf("   Target %d: %s (pid=%d)\n", j+1, backups[i].targets[j], backups[i].pids[j]);
        }
    }
}

// Ends backup
int end_backup(char* source, char** targets, int target_count) {
    if (!source || !targets || target_count <= 0) return EXIT_FAILURE;

    char real_src[PATH_MAX];
    if (!realpath(source, real_src)) ERR("realpath source");

    for (int i = 0; i < backup_count; i++) {
        if (strcmp(backups[i].source, real_src) != 0) continue;

        for (int t = 0; t < target_count; t++) {
            char real_t[PATH_MAX];
            if (!realpath(targets[t], real_t)) continue;

            for (int j = 0; j < backups[i].target_count; j++) {
                if (strcmp(backups[i].targets[j], real_t) == 0) {
                    if (backups[i].pids[j] > 0) {
                        kill(backups[i].pids[j], SIGTERM);
                        waitpid(backups[i].pids[j], NULL, 0);
                        backups[i].pids[j] = -1;
                    }

                    free(backups[i].targets[j]);
                    for (int k = j; k < backups[i].target_count - 1; k++) {
                        backups[i].targets[k] = backups[i].targets[k + 1];
                        backups[i].pids[k] = backups[i].pids[k + 1];
                    }
                    backups[i].target_count--;
                    j--; 
                }
            }
        }

        if (backups[i].target_count == 0) {
            free(backups[i].source);
            for (int k = i; k < backup_count - 1; k++) {
                backups[k] = backups[k + 1];
            }
            backup_count--;
            i--; 
        }
    }
    return EXIT_SUCCESS;
}

// Restores backup
int restore_backup(char* source, char* target) {
    char real_src[PATH_MAX], real_tgt[PATH_MAX];
    if (!realpath(source, real_src)) ERR("realpath source");
    if (!realpath(target, real_tgt)) ERR("realpath target");

    restore_copy(real_tgt, real_src, real_tgt, real_src);
    restore_remove(real_src, real_tgt, real_src);

    printf("Backup restored from %s to %s\n", real_tgt, real_src);
    return 0;
}
