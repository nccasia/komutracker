#include "instance.h"
#include "dirs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define PATH_SEP '/'
#endif

static int pid_path(char *out, size_t size) {
#ifdef _WIN32
    const char *value = getenv("LOCALAPPDATA");
    if (!value) value = getenv("APPDATA");
    if (!value) return -1;
    return snprintf(out, size, "%s%ckomutracker%ctracker.pid", value, PATH_SEP, PATH_SEP) >= (int)size ? -1 : 0;
#elif defined(__APPLE__)
    const char *value = getenv("HOME");
    if (!value) return -1;
    return snprintf(out, size, "%s/Library/Caches/komutracker/tracker.pid", value) >= (int)size ? -1 : 0;
#else
    const char *xdg = getenv("XDG_CACHE_HOME");
    const char *value = xdg && *xdg ? xdg : getenv("HOME");
    if (!value) return -1;
    return snprintf(out, size, "%s%ckomutracker%ctracker.pid", value, PATH_SEP, PATH_SEP) >= (int)size ? -1 : 0;
#endif
}

#ifdef _WIN32

static HANDLE win_lock_handle = INVALID_HANDLE_VALUE;

int instance_lock(void) {
    char path[2048];
    if (pid_path(path, sizeof(path))) return -1;
    if (dirs_create_parent(path)) return -1;
    HANDLE handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_HIDDEN, NULL);
    if (handle == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov = {0};
    if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)) {
        CloseHandle(handle);
        return 1;
    }
    SetFilePointer(handle, 0, NULL, FILE_BEGIN);
    SetEndOfFile(handle);
    char pid[16];
    int n = _snprintf(pid, sizeof(pid), "%d\n", (int)_getpid());
    if (n > 0) {
        DWORD written = 0;
        WriteFile(handle, pid, (DWORD)n, &written, NULL);
    }
    /* Keep the handle open for the lifetime of the process so the lock holds. */
    win_lock_handle = handle;
    return 0;
}

int instance_daemonize(void) { return -1; }

#else

static int lock_fd = -1;

int instance_lock(void) {
    char path[2048];
    if (pid_path(path, sizeof(path))) return -1;
    if (dirs_create_parent(path)) return -1;
    int fd = open(path, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return 1;
    }
    if (ftruncate(fd, 0) != 0) { close(fd); return -1; }
    char pid[16];
    int n = snprintf(pid, sizeof(pid), "%d\n", (int)getpid());
    if (n > 0) {
        ssize_t ignored = write(fd, pid, (size_t)n);
        (void)ignored;
    }
    lock_fd = fd;
    return 0;
}

int instance_daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    if (setsid() < 0) return -1;

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    if (chdir("/") != 0) return -1;
    umask(0);

    int devnull = open("/dev/null", O_RDWR);
    if (devnull < 0) return -1;
    if (dup2(devnull, STDIN_FILENO) < 0 ||
        dup2(devnull, STDOUT_FILENO) < 0 ||
        dup2(devnull, STDERR_FILENO) < 0) {
        close(devnull);
        return -1;
    }
    if (devnull > STDERR_FILENO) close(devnull);

    return 0;
}

#endif