#if !defined(_WIN32) && !defined(__APPLE__)
#include "window.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Display *display;
static Atom active_atom, pid_atom, name_atom, utf8_atom;
static int ignore_error(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

static unsigned char *property(Window window, Atom atom, Atom type, unsigned long *items) {
    Atom actual; int format; unsigned long after; unsigned char *data = NULL;
    if (XGetWindowProperty(display, window, atom, 0, 4096, False, type, &actual,
                           &format, items, &after, &data) != Success) return NULL;
    return data;
}

int window_init(void) {
    display = XOpenDisplay(NULL);
    if (!display) return -1;
    XSetErrorHandler(ignore_error);
    active_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    pid_atom = XInternAtom(display, "_NET_WM_PID", False);
    name_atom = XInternAtom(display, "_NET_WM_NAME", False);
    utf8_atom = XInternAtom(display, "UTF8_STRING", False);
    return 0;
}

int window_get_current(window_info *out) {
    strcpy(out->app, "unknown"); out->title[0] = '\0';
    unsigned long count = 0;
    unsigned char *active = property(DefaultRootWindow(display), active_atom, XA_WINDOW, &count);
    if (!active || !count) { if (active) XFree(active); return -1; }
    Window window = *(Window *)active; XFree(active);

    unsigned char *name = property(window, name_atom, utf8_atom, &count);
    if (name && count) snprintf(out->title, sizeof(out->title), "%.*s", (int)count, name);
    if (name) XFree(name);
    if (!out->title[0]) {
        char *legacy = NULL;
        if (XFetchName(display, window, &legacy) && legacy) {
            snprintf(out->title, sizeof(out->title), "%s", legacy); XFree(legacy);
        }
    }

    unsigned char *pid_data = property(window, pid_atom, XA_CARDINAL, &count);
    if (pid_data && count) {
        unsigned long pid = *(unsigned long *)pid_data;
        char path[128]; snprintf(path, sizeof(path), "/proc/%lu/comm", pid);
        FILE *file = fopen(path, "r");
        if (file) {
            if (fgets(out->app, sizeof(out->app), file)) out->app[strcspn(out->app, "\r\n")] = '\0';
            fclose(file);
        }
    }
    if (pid_data) XFree(pid_data);
    if (!strcmp(out->app, "unknown")) {
        XClassHint hint;
        if (XGetClassHint(display, window, &hint)) {
            if (hint.res_class) snprintf(out->app, sizeof(out->app), "%s", hint.res_class);
            if (hint.res_name) XFree(hint.res_name);
            if (hint.res_class) XFree(hint.res_class);
        }
    }
    return 0;
}

void window_cleanup(void) { if (display) XCloseDisplay(display); display = NULL; }
#endif
