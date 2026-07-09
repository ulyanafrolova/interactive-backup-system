#define _GNU_SOURCE
#include "parser.h"

char **split_command(char *input, int *argc) {
    input[strcspn(input, "\n")] = 0;

    char **argv = malloc(sizeof(char*) * 64);
    int count = 0;
    char *p = input;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char buf[1024];
        int len = 0;
        int in_quotes = 0; 
        if (*p == '"') {
            in_quotes = 1;
            p++;
        }
        while (*p) {
            if (*p == '\\') {
                p++;
                if (*p) buf[len++] = *p++;
            }
            else if (in_quotes && *p == '"') {
                p++;
                break;
            }
            else if (!in_quotes && (*p == ' ' || *p == '\t')) break;
            else buf[len++] = *p++;
        }
        buf[len] = '\0';
        argv[count++] = strdup(buf);
    }
    *argc = count;
    return argv;
}
