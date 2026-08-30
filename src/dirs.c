#include "dirs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define SEP '\\'
#define MKDIR(path) _mkdir(path)
#else
#include <errno.h>
#include <sys/stat.h>
#define SEP '/'
#define MKDIR(path) mkdir(path, 0700)
#endif

static int join(char *out, size_t size, const char *base, const char *suffix) {
    return snprintf(out, size, "%s%c%s", base, SEP, suffix) >= (int)size ? -1 : 0;
}

static int home(char *out, size_t size) {
#ifdef _WIN32
    const char *value = getenv("LOCALAPPDATA");
    if (!value) value = getenv("APPDATA");
#else
    const char *value = getenv("HOME");
#endif
    if (!value || strlen(value) >= size) return -1;
    strcpy(out, value);
    return 0;
}

int dirs_device_path(char *out, size_t size) {
    char base[1024];
#ifdef _WIN32
    if (home(base, sizeof(base))) return -1;
    return join(out, size, base, "komutracker\\.device_id");
#elif defined(__APPLE__)
    if (home(base, sizeof(base))) return -1;
    return join(out, size, base, "Library/Application Support/komutracker/.device_id");
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return join(out, size, xdg, "komutracker/.device_id");
    if (home(base, sizeof(base))) return -1;
    return join(out, size, base, ".local/share/komutracker/.device_id");
#endif
}

int dirs_token_path(char *out, size_t size) {
    char base[1024];
#ifdef _WIN32
    if (home(base, sizeof(base))) return -1;
    return join(out, size, base, "komutracker\\auth\\auth.tracker");
#elif defined(__APPLE__)
    if (home(base, sizeof(base))) return -1;
    return join(out, size, base, "Library/Caches/komutracker/auth/auth.tracker");
#else
    const char *xdg = getenv("XDG_CACHE_HOME");
    if (xdg && *xdg) return join(out, size, xdg, "komutracker/auth/auth.tracker");
    if (home(base, sizeof(base))) return -1;
    return join(out, size, base, ".cache/komutracker/auth/auth.tracker");
#endif
}

int dirs_create_parent(const char *path) {
    char copy[2048];
    size_t length = strlen(path);
    if (length >= sizeof(copy)) return -1;
    strcpy(copy, path);
    char *last = strrchr(copy, SEP);
    if (!last) return 0;
    *last = '\0';

    for (char *p = copy + 1; *p; p++) {
        if (*p != SEP) continue;
#ifdef _WIN32
        if (p == copy + 2 && copy[1] == ':') continue;
#endif
        *p = '\0';
        if (MKDIR(copy) != 0) {
#ifndef _WIN32
            if (errno != EEXIST) return -1;
#endif
        }
        *p = SEP;
    }
    if (MKDIR(copy) != 0) {
#ifndef _WIN32
        if (errno != EEXIST) return -1;
#endif
    }
    return 0;
}
