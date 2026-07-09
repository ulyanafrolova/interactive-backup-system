#include "functions.h"

int backup_count = 0;
Backup backups[MAX_BACKUPS];

// Creates a copy of a string src and returns a pointer
char* copy_string(const char* src) {
    size_t len = strlen(src);
    char* dst = malloc(len + 1);
    if (!dst) return NULL;
    memcpy(dst, src, len + 1);
    return dst;
}

// Checks if target is inside source
bool target_inside_source(const char* source, const char* target) {
    char real_src[PATH_MAX];
    char real_tgt[PATH_MAX];
    if (!realpath(source, real_src)) return false;
    if (!realpath(target, real_tgt)) return false;
    size_t len = strlen(real_src);
    return (strncmp(real_tgt, real_src, len) == 0 && (real_tgt[len] == '/' || real_tgt[len] == '\0'));
}

// Checks if backup already exists
bool backup_already_exists(const char* source, const char* target) {
    for (int i = 0; i < backup_count; i++) {
        if (strcmp(backups[i].source, source) == 0) {
            for (int j = 0; j < backups[i].target_count; j++) {
                if (strcmp(backups[i].targets[j], target) == 0) return true;
            }
        }
    }
    return false;
}

// Checks if we can add a backup
int prepare_target(const char* source, const char* target, char* real_target) {
    char abs_parent[PATH_MAX];
    char *slash = strrchr(target, '/');
    if (slash) {
        *slash = '\0';
        if (!realpath(target, abs_parent)) return -1;
        *slash = '/';
        if ((size_t)snprintf(real_target, PATH_MAX, "%s/%s", abs_parent, slash+1) >= PATH_MAX) ERR("real_target");
    }
    else {
        if (!realpath(".", abs_parent)) return -1;
        if ((size_t)snprintf(real_target, PATH_MAX, "%s/%s", abs_parent, target) >= PATH_MAX) ERR("real_target");
    }
    if (target_inside_source(source, real_target)) {
        fprintf(stderr, "Target directory cannot be inside source directory: %s\n", real_target);
        return -1;
    }
    if (backup_already_exists(source, real_target)) {
        fprintf(stderr, "Backup already exists: %s -> %s\n", source, real_target);
        return -1;
    }
    return 0;
}

// Checks if directory named path already exists
bool directory_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// Checks wether directory named path is empty or not
bool is_directory_empty(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return false;
    struct dirent* e;
    while ((e = readdir(dir))) {
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return true;
}

// Copies data from source file to destination file
int copy_file(const char* source, const char* destination) {
    if (!files_differ(source, destination)) return 0; 
    int in = open(source, O_RDONLY);
    if (in < 0) ERR("open");
    int out = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (out < 0) ERR("open");
    char buf[4096];
    ssize_t r;
    while ((r = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, r) != r) ERR("write");
    }
    close(in);
    close(out);
    return (r < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

// Copies symlink from src to tgt 
// src_root - source backup directory root
// tgt_root - target backup directory root
int handle_symlink(const char* src, const char* tgt, const char* src_root, const char* tgt_root) {
    char linkbuf[PATH_MAX];
    ssize_t len = readlink(src, linkbuf, sizeof(linkbuf) - 1);
    if (len < 0) return -1;
    linkbuf[len] = '\0';
    char newlink[PATH_MAX];
    if (linkbuf[0] == '/') {
        size_t root_len = strlen(src_root);
        if (strncmp(linkbuf, src_root, root_len) == 0 && (linkbuf[root_len] == '/' || linkbuf[root_len] == '\0')) {
            snprintf(newlink, sizeof(newlink), "%s%s", tgt_root, linkbuf + root_len);
        } 
        else strcpy(newlink, linkbuf);
    }
    else strcpy(newlink, linkbuf);
    unlink(tgt);
    return symlink(newlink, tgt);
}


// Recursively copies tree from src to dst
// src, dst are current directories
// src_root, dst_root are roots of source and destination trees, aren't be modified
int copy_tree(const char* src, const char* dst, const char* src_root, const char* dst_root) {
    DIR* dir = opendir(src);
    if (!dir) return -1;
    // Create directory for copy
    mkdir(dst, 0777);
    struct dirent* e;
    while ((e = readdir(dir))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char s[PATH_MAX], d[PATH_MAX];
        snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
        snprintf(d, sizeof(d), "%s/%s", dst, e->d_name);
        struct stat st;
        lstat(s, &st);
        if (S_ISDIR(st.st_mode)) copy_tree(s, d, src_root, dst_root);
        else if (S_ISREG(st.st_mode)) copy_file(s, d);
        else if (S_ISLNK(st.st_mode)) handle_symlink(s, d, src_root, dst_root);
    }
    closedir(dir);
    return 0;
}

// Checks if src file and dst file differ
bool files_differ(const char* src, const char* dst) {
    struct stat s1, s2;
    if (lstat(dst, &s2) != 0) return true;  
    if (lstat(src, &s1) != 0) return false;
    return s1.st_mtime != s2.st_mtime || s1.st_size != s2.st_size;
}

// Restores a copy recursively from backup to source
// tgt - path in backup tree
// src - corresponding path in source tree
// tgt_root - root of backup tree (used for adjusting absolute symlinks)
// src_root - root of source tree (used for adjusting absolute symlinks)
int restore_copy(const char* tgt, const char* src, const char* tgt_root, const char* src_root) {
    struct stat st;
    if (lstat(tgt, &st) < 0) return -1;
    if (S_ISDIR(st.st_mode)) mkdir(src, st.st_mode);
    else if (S_ISREG(st.st_mode)) {
        if (files_differ(tgt, src)) copy_file(tgt, src);
    }
    else if (S_ISLNK(st.st_mode)) handle_symlink(tgt, src, tgt_root, src_root);
    if (!S_ISDIR(st.st_mode)) return 0;

    DIR* dir = opendir(tgt);
    if (!dir) return 0;

    struct dirent* e;
    while ((e = readdir(dir))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char t[PATH_MAX], s[PATH_MAX];
        snprintf(t, sizeof(t), "%s/%s", tgt, e->d_name);
        snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
        restore_copy(t, s, tgt_root, src_root);
    }
    closedir(dir);
    return 0;
}

// Recursively removes files/directories from srs
// tgt_root - target(backup) directory root
// src_root - source root directory
int restore_remove(const char* src, const char* tgt_root, const char* src_root) {
    DIR* dir = opendir(src);
    if (!dir) return 0;
    struct dirent* e;
    while ((e = readdir(dir))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char s[PATH_MAX], t[PATH_MAX];
        snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
        snprintf(t, sizeof(t), "%s/%s", tgt_root, s + strlen(src_root) + 1);
        struct stat st_src;
        if (lstat(s, &st_src) < 0) continue;
        struct stat st_tgt;
        if (lstat(t, &st_tgt) < 0) {
            if (S_ISDIR(st_src.st_mode)) {
                restore_remove(s, tgt_root, src_root);
                rmdir(s);
            } else {
                unlink(s);
            }
        }
        else if (S_ISDIR(st_src.st_mode)) {
            restore_remove(s, tgt_root, src_root);
        }
    }
    closedir(dir);
    return 0;
}
