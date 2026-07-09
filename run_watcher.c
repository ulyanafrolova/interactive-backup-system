#include "run_watcher.h"
#include "functions.h"

static volatile sig_atomic_t watcher_stop = 0;

static void watcher_sigterm_handler(int sig) {
    watcher_stop = 1;
}

void run_watcher_loop(const char* source_root, const char* copy_root) {
    Watch watches[1024];
    int watch_count = 0;

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = watcher_sigterm_handler;
    if (-1 == sigaction(SIGTERM, &act, NULL)) ERR("sigaction");
    if (-1 == sigaction(SIGINT, &act, NULL)) ERR("sigaction");
    // Initialize inotify instance
    int fd = inotify_init();
    if (fd < 0) ERR("inotify_init");
    add_watch_recursive(fd, source_root, watches, &watch_count);
    char buf[4096];
    while (!watcher_stop) {
        ssize_t len = read(fd, buf, sizeof(buf));
        if (len < 0) {
            if (errno == EINTR && watcher_stop) break;
            continue;
        }
        // Iterate over all events in the buffer
        for (char* ptr = buf; ptr < buf + len; ) {
            struct inotify_event* ev = (struct inotify_event*) ptr;
            char source[PATH_MAX], copy[PATH_MAX];
            char* base = NULL;
            for (int i=0; i<watch_count; i++) {
                if (watches[i].wd == ev->wd) base = watches[i].path;
            }
            if (!base) {
                ptr += sizeof(struct inotify_event) + ev->len;
                continue;
            }
            if ((size_t)snprintf(source, sizeof(source), "%s/%s", base, ev->name) >= sizeof(source)) ERR("path too long");
            snprintf(copy, sizeof(copy), "%s/%s", copy_root, source+strlen(source_root)+1);
            if (ev->mask & IN_CREATE || ev->mask & IN_MOVED_TO) {
                sync_create(source, copy);
                struct stat st;
                if (lstat(source, &st) == 0 && S_ISDIR(st.st_mode)) add_watch_recursive(fd, source, watches, &watch_count);
            }
            else if (ev->mask & IN_MODIFY) sync_create(source, copy);
            else if (ev->mask & IN_DELETE || ev->mask & IN_MOVED_FROM) sync_delete_recursive(copy);
            else if ((ev->mask & IN_DELETE_SELF) || (ev->mask & IN_MOVE_SELF))  {
                sync_delete_recursive(copy);
                // Delete watch from watches array
                for (int i = 0; i < watch_count; i++) {
                    if (watches[i].wd == ev->wd) {
                        for (int j = i; j < watch_count - 1; j++) {
                            watches[j] = watches[j + 1];
                        }
                        watch_count--;
                        break;
                    }
                }
                // Stop watcher only if it is a root directory
                if (strcmp(base, source_root) == 0) {
                    watcher_stop = 1;
                }
            }
            ptr += sizeof(struct inotify_event) + ev->len;
        }
    }
    close(fd);
}

// Inotify does not monitor subdirectories, so we add watch recursively for path and each its subdirectory
void add_watch_recursive(int fd, const char* path, Watch* watches, int* watch_count) {

    int wd = inotify_add_watch(fd, path, IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF);
    if (wd < 0) ERR("inotify_add_watch");
    watches[*watch_count].wd = wd;
    strncpy(watches[*watch_count].path, path, PATH_MAX - 1);
    watches[*watch_count].path[PATH_MAX - 1] = '\0';
    (*watch_count)++;

    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* e;
    while ((e = readdir(dir))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;

        char subdirectory[PATH_MAX];
        snprintf(subdirectory, sizeof(subdirectory), "%s/%s", path, e->d_name);

        struct stat st;
        if (lstat(subdirectory, &st) == 0 && S_ISDIR(st.st_mode)) add_watch_recursive(fd, subdirectory, watches, watch_count);
    }
    closedir(dir);
}

// Synchronise creation
void sync_create(const char* source, const char* target) {
    struct stat st;
    lstat(source, &st);
    // Directory
    if (S_ISDIR(st.st_mode)) {
        mkdir(target, st.st_mode);
    }
    // Regular file
    else if (S_ISREG(st.st_mode)) {
        copy_file(source, target);
    }
    // Symbolic link
    else if (S_ISLNK(st.st_mode)) {
        char buf[PATH_MAX];
        ssize_t len = readlink(source, buf, sizeof(buf)-1);
        buf[len] = 0;
        symlink(buf, target);
    }
}

// Synchronise removal
void sync_delete_recursive(const char* target) {
    struct stat st;
    if (lstat(target, &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(target);
        if (!dir) return;
        struct dirent* e;
        char path[PATH_MAX];
        while ((e = readdir(dir))) {
            if (!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
            snprintf(path, sizeof(path), "%s/%s", target, e->d_name);
            sync_delete_recursive(path);
        }
        closedir(dir);
        rmdir(target);
    } 
    // Delete file or symbolic link
    else {
        unlink(target);
    }
}
