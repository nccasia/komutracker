#if !defined(_WIN32) && !defined(__APPLE__)
#include "idle.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>

#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

#ifdef KOMUTRACKER_HAVE_WAYLAND
#include <wayland-client.h>
#include "ext_idle_notify_v1_client_protocol.h"

static struct wl_display *wl_disp = NULL;
static struct ext_idle_notifier_v1 *wl_notifier = NULL;
static struct ext_idle_notification_v1 *wl_notification = NULL;
static struct wl_seat *wl_seat_obj = NULL;
static bool wl_is_idle = false;
static double wl_last_input = 0.0;
static bool using_wayland = false;

static double get_monotonic_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void wl_handle_idled(void *data, struct ext_idle_notification_v1 *n) {
    (void)data; (void)n;
    wl_is_idle = true;
    wl_last_input = get_monotonic_now() - 1.0;
}

static void wl_handle_resumed(void *data, struct ext_idle_notification_v1 *n) {
    (void)data; (void)n;
    wl_is_idle = false;
    wl_last_input = get_monotonic_now();
}

static const struct ext_idle_notification_v1_listener wl_listener = {
    .idled = wl_handle_idled,
    .resumed = wl_handle_resumed,
};

static void wl_registry_global(void *data, struct wl_registry *registry,
                               uint32_t name, const char *interface, uint32_t version) {
    (void)data; (void)version;
    if (strcmp(interface, ext_idle_notifier_v1_interface.name) == 0) {
        wl_notifier = wl_registry_bind(registry, name, &ext_idle_notifier_v1_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        wl_seat_obj = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
}

static void wl_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener wl_reg_listener = {
    .global = wl_registry_global,
    .global_remove = wl_registry_global_remove,
};

static int init_wayland_idle(void) {
    wl_disp = wl_display_connect(NULL);
    if (!wl_disp) return -1;

    struct wl_registry *reg = wl_display_get_registry(wl_disp);
    if (!reg) {
        wl_display_disconnect(wl_disp);
        wl_disp = NULL;
        return -1;
    }

    wl_registry_add_listener(reg, &wl_reg_listener, NULL);
    wl_display_roundtrip(wl_disp);
    wl_registry_destroy(reg);

    if (!wl_notifier) {
        if (wl_seat_obj) { wl_seat_destroy(wl_seat_obj); wl_seat_obj = NULL; }
        wl_display_disconnect(wl_disp);
        wl_disp = NULL;
        return -1;
    }

    wl_last_input = get_monotonic_now();
    wl_is_idle = false;
    wl_notification = ext_idle_notifier_v1_get_idle_notification(wl_notifier, 1000, wl_seat_obj);
    if (!wl_notification) {
        ext_idle_notifier_v1_destroy(wl_notifier);
        wl_notifier = NULL;
        if (wl_seat_obj) { wl_seat_destroy(wl_seat_obj); wl_seat_obj = NULL; }
        wl_display_disconnect(wl_disp);
        wl_disp = NULL;
        return -1;
    }

    ext_idle_notification_v1_add_listener(wl_notification, &wl_listener, NULL);
    wl_display_roundtrip(wl_disp);
    using_wayland = true;
    return 0;
}

static void poll_wayland_events(void) {
    if (!wl_disp) return;
    while (wl_display_prepare_read(wl_disp) != 0) {
        wl_display_dispatch_pending(wl_disp);
    }
    wl_display_flush(wl_disp);
    struct pollfd pfd = { .fd = wl_display_get_fd(wl_disp), .events = POLLIN };
    if (poll(&pfd, 1, 0) > 0) {
        wl_display_read_events(wl_disp);
        wl_display_dispatch_pending(wl_disp);
    } else {
        wl_display_cancel_read(wl_disp);
    }
}

static double get_wayland_idle_seconds(void) {
    poll_wayland_events();
    double now = get_monotonic_now();
    if (wl_is_idle) {
        return now - wl_last_input;
    } else {
        double elapsed = now - wl_last_input;
        return elapsed < 1.0 ? elapsed : 0.0;
    }
}

static void cleanup_wayland(void) {
    if (wl_notification) { ext_idle_notification_v1_destroy(wl_notification); wl_notification = NULL; }
    if (wl_notifier) { ext_idle_notifier_v1_destroy(wl_notifier); wl_notifier = NULL; }
    if (wl_seat_obj) { wl_seat_destroy(wl_seat_obj); wl_seat_obj = NULL; }
    if (wl_disp) { wl_display_disconnect(wl_disp); wl_disp = NULL; }
    using_wayland = false;
}
#endif

/* X11 Backend */
static Display *x_display = NULL;
static XScreenSaverInfo *x_info = NULL;

static int init_x11_idle(void) {
    x_display = XOpenDisplay(NULL);
    if (!x_display) return -1;

    int event_base = 0, error_base = 0;
    if (!XScreenSaverQueryExtension(x_display, &event_base, &error_base)) {
        /* X server / Xwayland does NOT support MIT-SCREEN-SAVER */
        XCloseDisplay(x_display);
        x_display = NULL;
        return -1;
    }

    x_info = XScreenSaverAllocInfo();
    if (!x_info) {
        XCloseDisplay(x_display);
        x_display = NULL;
        return -1;
    }
    return 0;
}

int idle_init(void) {
#ifdef KOMUTRACKER_HAVE_WAYLAND
    if (getenv("WAYLAND_DISPLAY") && init_wayland_idle() == 0) {
        return 0;
    }
#endif

    if (init_x11_idle() == 0) {
        return 0;
    }

    if (getenv("WAYLAND_DISPLAY")) {
        fprintf(stderr, "Wayland idle detection unavailable (ext-idle-notify-v1 not supported by compositor and MIT-SCREEN-SAVER missing in Xwayland)\n");
    } else {
        fprintf(stderr, "Cannot initialize idle detection: X11 MIT-SCREEN-SAVER extension not supported on display\n");
    }
    return -1;
}

double idle_seconds(void) {
#ifdef KOMUTRACKER_HAVE_WAYLAND
    if (using_wayland) {
        return get_wayland_idle_seconds();
    }
#endif

    if (!x_display || !x_info) return -1.0;
    if (!XScreenSaverQueryInfo(x_display, DefaultRootWindow(x_display), x_info)) return -1.0;
    return (double)x_info->idle / 1000.0;
}

void idle_cleanup(void) {
#ifdef KOMUTRACKER_HAVE_WAYLAND
    if (using_wayland) {
        cleanup_wayland();
        return;
    }
#endif
    if (x_info) { XFree(x_info); x_info = NULL; }
    if (x_display) { XCloseDisplay(x_display); x_display = NULL; }
}
#endif
