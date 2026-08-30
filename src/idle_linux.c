#if !defined(_WIN32) && !defined(__APPLE__)
#include "idle.h"
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

static Display *display;
static XScreenSaverInfo *info;

int idle_init(void) {
    if (getenv("WAYLAND_DISPLAY") && !getenv("DISPLAY")) {
        fprintf(stderr, "Wayland session has no X11 display; no supported global idle API is available\n");
        return -1;
    }
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X11 display for idle detection\n");
        return -1;
    }
    info = XScreenSaverAllocInfo();
    return info ? 0 : -1;
}

double idle_seconds(void) {
    if (!XScreenSaverQueryInfo(display, DefaultRootWindow(display), info)) return -1.0;
    return (double)info->idle / 1000.0;
}

void idle_cleanup(void) {
    if (info) XFree(info);
    if (display) XCloseDisplay(display);
}
#endif
