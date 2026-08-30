#if !defined(_WIN32) && !defined(__APPLE__)
#include "window.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static Display *display;
static Atom active_atom, pid_atom, name_atom, utf8_atom, wm_state_atom;
static int ignore_error(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

static unsigned char *property(Window window, Atom atom, Atom type, unsigned long *items) {
    Atom actual; int format; unsigned long after; unsigned char *data = NULL;
    if (XGetWindowProperty(display, window, atom, 0, 4096, False, type, &actual,
                           &format, items, &after, &data) != Success) return NULL;
    return data;
}

/* Resolve the name of the process that owns `pid`. Prefer the full executable
   path (/proc/<pid>/exe) because /proc/<pid>/comm truncates to 15 chars.
   Returns 0 on success, -1 if the PID is not resolvable from this namespace
   (e.g. Flatpak/Snap processes that report a sandbox-side PID). */
static int app_from_proc(unsigned long pid, char *out, size_t size) {
    char path[128], link[4096];
    snprintf(path, sizeof(path), "/proc/%lu/exe", pid);
    ssize_t length = readlink(path, link, sizeof(link) - 1);
    if (length > 0) {
        link[length] = '\0';
        /* readlink may append " (deleted)" for a replaced binary. */
        char *deleted = strstr(link, " (deleted)");
        if (deleted) *deleted = '\0';
        const char *base = strrchr(link, '/');
        base = base ? base + 1 : link;
        if (*base) {
            snprintf(out, size, "%.*s", (int)(length - (base - link)), base);
            return 0;
        }
    }

    snprintf(path, sizeof(path), "/proc/%lu/comm", pid);
    FILE *file = fopen(path, "r");
    if (file) {
        if (fgets(out, size, file)) out[strcspn(out, "\r\n")] = '\0';
        fclose(file);
        if (out[0]) return 0;
    }
    return -1;
}

/* Pick the window to report on: prefer the EWMH _NET_ACTIVE_WINDOW hint, and
   fall back to the current input focus. The focus fallback covers minimal WMs
   that never update _NET_ACTIVE_WINDOW. */
static Window resolve_target(void) {
    Window target = None;

    unsigned long count = 0;
    unsigned char *active = property(DefaultRootWindow(display), active_atom, XA_WINDOW, &count);
    if (active && count) target = *(Window *)active;
    if (active) XFree(active);

    if (target != None && target != PointerRoot) return target;

    int revert = RevertToNone;
    XGetInputFocus(display, &target, &revert);
    if (target == None || target == PointerRoot) return None;
    return target;
}

/* The focused/active window is frequently a child of the top-level window that
   actually carries _NET_WM_NAME / _NET_WM_PID / WM_CLASS. Walk up towards the
   root until we reach a window exposing any of those, or the root itself. */
static Window resolve_toplevel(Window window) {
    for (int depth = 0; window && window != DefaultRootWindow(display) && depth < 8; depth++) {
        unsigned long count = 0;
        unsigned char *pid = property(window, pid_atom, XA_CARDINAL, &count);
        unsigned char *name = property(window, name_atom, utf8_atom, &count);
        unsigned char *state = property(window, wm_state_atom, wm_state_atom, &count);
        XClassHint hint = {0};
        int has_class = XGetClassHint(display, window, &hint);
        int useful = (pid && count) || (name && count) || (state && count) || has_class;
        if (pid) XFree(pid);
        if (name) XFree(name);
        if (state) XFree(state);
        if (has_class) {
            if (hint.res_name) XFree(hint.res_name);
            if (hint.res_class) XFree(hint.res_class);
        }
        if (useful) return window;

        Window root, parent, *children = NULL; unsigned int nchildren = 0;
        if (!XQueryTree(display, window, &root, &parent, &children, &nchildren)) return window;
        if (children) XFree(children);
        if (!parent) return window;
        window = parent;
    }
    return window;
}

int window_init(void) {
    display = XOpenDisplay(NULL);
    if (!display) return -1;
    XSetErrorHandler(ignore_error);
    active_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    pid_atom = XInternAtom(display, "_NET_WM_PID", False);
    name_atom = XInternAtom(display, "_NET_WM_NAME", False);
    utf8_atom = XInternAtom(display, "UTF8_STRING", False);
    wm_state_atom = XInternAtom(display, "WM_STATE", False);
    return 0;
}

int window_get_current(window_info *out) {
    strcpy(out->app, "unknown"); out->title[0] = '\0';

    Window window = resolve_target();
    if (!window) return -1;
    window = resolve_toplevel(window);

    /* Title: EWMH _NET_WM_NAME (UTF-8), else the legacy WM_NAME. */
    unsigned long count = 0;
    unsigned char *name = property(window, name_atom, utf8_atom, &count);
    if (name && count) snprintf(out->title, sizeof(out->title), "%.*s", (int)count, name);
    if (name) XFree(name);
    if (!out->title[0]) {
        char *legacy = NULL;
        if (XFetchName(display, window, &legacy) && legacy) {
            snprintf(out->title, sizeof(out->title), "%s", legacy); XFree(legacy);
        }
    }

    /* App: _NET_WM_PID -> /proc/<pid>/exe or comm, then WM_CLASS class, then
       WM_CLASS instance as a last resort. */
    unsigned char *pid_data = property(window, pid_atom, XA_CARDINAL, &count);
    if (pid_data && count) {
        unsigned long pid = *(unsigned long *)pid_data;
        app_from_proc(pid, out->app, sizeof(out->app));
    }
    if (pid_data) XFree(pid_data);

    if (!strcmp(out->app, "unknown")) {
        XClassHint hint = {0};
        if (XGetClassHint(display, window, &hint)) {
            if (hint.res_class) snprintf(out->app, sizeof(out->app), "%s", hint.res_class);
            else if (hint.res_name) snprintf(out->app, sizeof(out->app), "%s", hint.res_name);
            if (hint.res_name) XFree(hint.res_name);
            if (hint.res_class) XFree(hint.res_class);
        }
    }
    return 0;
}

void window_cleanup(void) { if (display) XCloseDisplay(display); display = NULL; }
#endif