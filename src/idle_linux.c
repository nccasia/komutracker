#if !defined(_WIN32) && !defined(__APPLE__)
#include "idle.h"
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

typedef enum {
    IDLE_BACKEND_NONE,
    IDLE_BACKEND_MUTTER,
    IDLE_BACKEND_FREEDESKTOP,
    IDLE_BACKEND_X11,
} idle_backend;

static idle_backend backend;
static GDBusConnection *session_bus;
static Display *display;
static XScreenSaverInfo *x11_scr_saver_info;

static int is_wayland_session(void) {
    const char *session_type = getenv("XDG_SESSION_TYPE");
    return getenv("WAYLAND_DISPLAY") ||
        (session_type && !strcmp(session_type, "wayland"));
}

static double dbus_idle_seconds(const char *name, const char *path, const char *interface, const char *method, const GVariantType *reply_type, int milliseconds) {
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_sync(session_bus, name, path, interface, method, NULL, reply_type, G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    if (!reply) {
        if (error) g_error_free(error);
        return -1.0;
    }

    double seconds = -1.0;
    if (milliseconds) {
        guint64 value = 0;
        g_variant_get(reply, "(t)", &value);
        seconds = (double)value / 1000.0;
    } else {
        guint32 value = 0;
        g_variant_get(reply, "(u)", &value);
        seconds = (double)value;
    }
    g_variant_unref(reply);
    return seconds;
}

static double mutter_idle_seconds(void) {
    return dbus_idle_seconds(
        "org.gnome.Mutter.IdleMonitor", "/org/gnome/Mutter/IdleMonitor/Core",
        "org.gnome.Mutter.IdleMonitor", "GetIdletime",
        G_VARIANT_TYPE("(t)"), 1);
}

static double freedesktop_idle_seconds(void) {
    return dbus_idle_seconds(
        "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
        "org.freedesktop.ScreenSaver", "GetSessionIdleTime",
        G_VARIANT_TYPE("(u)"), 0);
}

static int init_wayland(void) {
    GError *error = NULL;
    session_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!session_bus) {
        if (error) g_error_free(error);
        return -1;
    }

    if (mutter_idle_seconds() >= 0.0) {
        backend = IDLE_BACKEND_MUTTER;
        return 0;
    }
    if (freedesktop_idle_seconds() >= 0.0) {
        backend = IDLE_BACKEND_FREEDESKTOP;
        return 0;
    }

    g_object_unref(session_bus);
    session_bus = NULL;
    return -1;
}

static int init_x11(void) {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X11 display for idle detection\n");
        return -1;
    }
    x11_scr_saver_info = XScreenSaverAllocInfo();
    if (!x11_scr_saver_info) {
        XCloseDisplay(display);
        display = NULL;
        return -1;
    }
    backend = IDLE_BACKEND_X11;
    return 0;
}

int idle_init(void) {
    backend = IDLE_BACKEND_NONE;

    /* Native Wayland input is not visible to XScreenSaver. Prefer the desktop
       session's D-Bus idle API even when an XWayland DISPLAY is present. */
    if (is_wayland_session() && init_wayland() == 0) return 0;
    if (getenv("DISPLAY") && init_x11() == 0) return 0;

    fprintf(stderr,
        "No supported idle API is available (tried Wayland session D-Bus and XScreenSaver)\n");
    return -1;
}

double idle_seconds(void) {
    switch (backend) {
    case IDLE_BACKEND_MUTTER:
        return mutter_idle_seconds();
    case IDLE_BACKEND_FREEDESKTOP:
        return freedesktop_idle_seconds();
    case IDLE_BACKEND_X11:
        if (!XScreenSaverQueryInfo(display, DefaultRootWindow(display), x11_scr_saver_info)) return -1.0;
        return (double)x11_scr_saver_info->idle / 1000.0;
    default:
        return -1.0;
    }
}

void idle_cleanup(void) {
    if (x11_scr_saver_info) XFree(x11_scr_saver_info);
    if (display) XCloseDisplay(display);
    if (session_bus) g_object_unref(session_bus);
    x11_scr_saver_info = NULL;
    display = NULL;
    session_bus = NULL;
    backend = IDLE_BACKEND_NONE;
}
#endif
