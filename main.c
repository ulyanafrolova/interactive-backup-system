#define _GNU_SOURCE
#include "parser.h"
#include "backup.h"
#include "functions.h"

volatile sig_atomic_t got_signal = 0;

void sig_handler(int sig) {
    got_signal = sig;
}

void print_commands() {
    printf("Available commands:\n");
    printf("1. add <source> <target1> <target2> ... - create new backup\n");
    printf("2. end <source> <target1> <target2> ... - stop monitoring backup\n");
    printf("3. list - list active backups\n");
    printf("4. restore <source> <target> - restore backup\n");
    printf("5. exit - exit program\n");
}

void usage(char *pname) {
    printf("Usage: %s\n", pname);
    print_commands();
}

void set_handler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL)) ERR("sigaction");
}

void ignore_signals() {
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    for (int sig =1; sig<NSIG; sig++) {
        if (sig == SIGINT || sig == SIGTERM) continue;
        if (-1 == sigaction(sig, &act, NULL)) ERR("sigaction");
    }
}

void kill_process() {
    for (int i = 0; i < backup_count; i++) {
        for (int j = 0; j < backups[i].target_count; j++) {
            if (backups[i].pids[j] > 0) {
                kill(backups[i].pids[j], SIGTERM);
                waitpid(backups[i].pids[j], NULL, 0);
            }
            free(backups[i].targets[j]);
        }
        free(backups[i].source);
    }
    backup_count = 0;
}

int main(int argc, char* argv[])
{     
    print_commands();
    char buffer[1024];
    ignore_signals();
    set_handler(sig_handler, SIGINT);
    set_handler(sig_handler, SIGTERM);
    while (!got_signal) {      
        printf("> ");
        if (!fgets(buffer, sizeof(buffer), stdin)) break;
        int argc_cmd = 0;
        char** args = split_command(buffer, &argc_cmd);
        if (!args || argc_cmd == 0) continue;

        if (strcmp(args[0], "add") == 0) {
            if (argc_cmd < 3) usage(argv[0]);
            else {
                char* source = args[1];
                char** targets = &args[2];
                int target_count = argc_cmd - 2;
                add_backup(source, targets, target_count);
            }
        }
        else if (strcmp(args[0], "end") == 0) {
            if (argc_cmd < 3) usage(argv[0]);
            else {
                char* source = args[1];
                char** targets = &args[2];
                int target_count = argc_cmd - 2;
                end_backup(source, targets, target_count);
            }
        }
        else if (strcmp(args[0], "list") == 0) {
            list_backups();
        }
        else if (strcmp(args[0], "restore") == 0) {
            if (argc_cmd != 3) usage(argv[0]);
            else {
                restore_backup(args[1], args[2]);
            }
        }
        else if (strcmp(args[0], "exit") == 0) {
            kill_process();
            exit(EXIT_SUCCESS);
        }
        else {
            printf("Unknown command\n");
            print_commands();
        }
        for (int i = 0; i < argc_cmd; i++) free(args[i]);
        free(args);
    }
    if (got_signal == SIGINT || got_signal == SIGTERM) kill_process();
    return EXIT_SUCCESS;
}
