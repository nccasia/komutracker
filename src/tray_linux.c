#if !defined(_WIN32) && !defined(__APPLE__)

#include "tray.h"

#include <gtk/gtk.h>
#ifdef KOMUTRACKER_USE_AYATANA
#include <libayatana-appindicator/app-indicator.h>
#else
#include <libappindicator/app-indicator.h>
#endif

#include "dirs.h"
#include "tray_icon_data.h"

#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef KOMUTRACKER_TRAY_ICON_PATH
#define KOMUTRACKER_TRAY_ICON_PATH "/usr/share/pixmaps/komutracker-tray.png"
#endif

static tray_callbacks callbacks;
static AppIndicator *indicator;
static GtkWidget *menu;
static GtkWidget *account_item;
static GtkWidget *today_item;
static GtkWidget *status_item;
static GtkWidget *dashboard_item;
static GtkWidget *auth_item;
static GtkWidget *logout_item;

static int check_icon_candidate(const char *path, char *out, size_t size) {
    if (!path || !*path) return 0;
    if (access(path, R_OK) == 0) {
        if (realpath(path, out)) return 1;
        snprintf(out, size, "%s", path);
        return 1;
    }
    return 0;
}

static void resolve_tray_icon_path(char *out, size_t size) {
    // 1. Check configured/installed system path
    if (check_icon_candidate(KOMUTRACKER_TRAY_ICON_PATH, out, size)) return;

    // 2. Check XDG data directory (~/.local/share/pixmaps/komutracker-tray.png)
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    char candidate[PATH_MAX];
    if (xdg && *xdg) {
        snprintf(candidate, sizeof(candidate), "%s/pixmaps/komutracker-tray.png", xdg);
        if (check_icon_candidate(candidate, out, size)) return;
    }
    if (home && *home) {
        snprintf(candidate, sizeof(candidate), "%s/.local/share/pixmaps/komutracker-tray.png", home);
        if (check_icon_candidate(candidate, out, size)) return;
    }

    // 3. Check relative to current executable (/proc/self/exe)
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = '\0';
        char exe_copy[PATH_MAX];
        strncpy(exe_copy, exe, sizeof(exe_copy));
        char *dir = dirname(exe_copy);
        if (dir) {
            snprintf(candidate, sizeof(candidate), "%s/../packaging/media/logo_tray.png", dir);
            if (check_icon_candidate(candidate, out, size)) return;

            snprintf(candidate, sizeof(candidate), "%s/../share/pixmaps/komutracker-tray.png", dir);
            if (check_icon_candidate(candidate, out, size)) return;

            snprintf(candidate, sizeof(candidate), "%s/packaging/media/logo_tray.png", dir);
            if (check_icon_candidate(candidate, out, size)) return;

            snprintf(candidate, sizeof(candidate), "%s/logo_tray.png", dir);
            if (check_icon_candidate(candidate, out, size)) return;
        }
    }

    // 4. Check relative to current working directory
    if (check_icon_candidate("packaging/media/logo_tray.png", out, size)) return;
    if (check_icon_candidate("public/logo.png", out, size)) return;

    // 5. Fallback: write embedded icon to user's local pixmaps directory
    char fallback_path[PATH_MAX];
    fallback_path[0] = '\0';
    if (xdg && *xdg) {
        snprintf(fallback_path, sizeof(fallback_path), "%s/pixmaps/komutracker-tray.png", xdg);
    } else if (home && *home) {
        snprintf(fallback_path, sizeof(fallback_path), "%s/.local/share/pixmaps/komutracker-tray.png", home);
    }
    if (fallback_path[0] && dirs_create_parent(fallback_path) == 0) {
        FILE *f = fopen(fallback_path, "wb");
        if (f) {
            fwrite(komutracker_tray_icon_png, 1, komutracker_tray_icon_png_len, f);
            fclose(f);
            if (check_icon_candidate(fallback_path, out, size)) return;
        }
    }

    // Ultimate fallback
    snprintf(out, size, "%s", KOMUTRACKER_TRAY_ICON_PATH);
}

static void auth_action(GtkMenuItem *item, gpointer context) {
    (void)item;
    (void)context;
    if (callbacks.auth_action) callbacks.auth_action(callbacks.context);
}

static void logout_action(GtkMenuItem *item, gpointer context) {
    (void)item;
    (void)context;
    if (callbacks.logout) callbacks.logout(callbacks.context);
}

static void dashboard_action(GtkMenuItem *item, gpointer context) {
    (void)item;
    (void)context;
    if (callbacks.open_dashboard) callbacks.open_dashboard(callbacks.context);
}

static void quit_action(GtkMenuItem *item, gpointer context) {
    (void)item;
    (void)context;
    if (callbacks.quit) callbacks.quit(callbacks.context);
}

static GtkWidget *new_item(const char *label, GCallback callback) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    if (callback) g_signal_connect(item, "activate", callback, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    return item;
}

int tray_init(const tray_callbacks *provided_callbacks) {
    callbacks = *provided_callbacks;
    if (!gtk_init_check(NULL, NULL)) return -1;

    char icon_path[PATH_MAX];
    resolve_tray_icon_path(icon_path, sizeof(icon_path));

    char icon_dir_buf[PATH_MAX];
    strncpy(icon_dir_buf, icon_path, sizeof(icon_dir_buf));
    char *icon_dir = dirname(icon_dir_buf);

    indicator = app_indicator_new_with_path("komutracker", icon_path,
                                            APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
                                            icon_dir);
    if (!indicator) return -1;
    app_indicator_set_icon_theme_path(indicator, icon_dir);
    app_indicator_set_icon_full(indicator, icon_path, "KomuTracker");
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

    menu = gtk_menu_new();
    account_item = new_item("Not Logged In", NULL);
    today_item = new_item("", NULL);
    status_item = new_item("Starting…", NULL);
    gtk_widget_set_sensitive(account_item, FALSE);
    gtk_widget_set_sensitive(today_item, FALSE);
    gtk_widget_set_sensitive(status_item, FALSE);
    gtk_widget_hide(today_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    dashboard_item = new_item("Open Dashboard", G_CALLBACK(dashboard_action));
    auth_item = new_item("Log In", G_CALLBACK(auth_action));
    logout_item = new_item("Log Out", G_CALLBACK(logout_action));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    new_item("Quit KomuTracker", G_CALLBACK(quit_action));

    gtk_widget_show_all(menu);
    gtk_widget_hide(today_item);
    app_indicator_set_menu(indicator, GTK_MENU(menu));
    return 0;
}

void tray_update(const tray_view *view) {
    gtk_menu_item_set_label(GTK_MENU_ITEM(account_item), view->account_name);
    gtk_menu_item_set_label(GTK_MENU_ITEM(status_item), view->status_text);
    gtk_widget_set_sensitive(dashboard_item, view->logged_in);

    if (view->logged_in && view->today_time[0]) {
        gtk_menu_item_set_label(GTK_MENU_ITEM(today_item), view->today_time);
        gtk_widget_show(today_item);

        const char *label_text = view->today_time;
        if (strncmp(label_text, "Today: ", 7) == 0) label_text += 7;
        app_indicator_set_label(indicator, label_text, "00h 00m");

        char title[512];
        snprintf(title, sizeof(title), "%s — %s (%s)",
                 view->account_name, view->today_time, view->status_text);
        app_indicator_set_title(indicator, title);
    } else {
        gtk_widget_hide(today_item);
        app_indicator_set_label(indicator, "", "");
        app_indicator_set_title(indicator, "KomuTracker");
    }

    if (view->status == TRAY_AUTHENTICATING) {
        gtk_menu_item_set_label(GTK_MENU_ITEM(auth_item), "Cancel Login");
        gtk_widget_show(auth_item);
        gtk_widget_hide(logout_item);
    } else if (view->logged_in) {
        gtk_widget_hide(auth_item);
        gtk_widget_show(logout_item);
    } else {
        gtk_menu_item_set_label(GTK_MENU_ITEM(auth_item), "Log In");
        gtk_widget_show(auth_item);
        gtk_widget_hide(logout_item);
    }
}

void tray_poll(void) {
    while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
}

void tray_cleanup(void) {
    if (indicator) app_indicator_set_status(indicator, APP_INDICATOR_STATUS_PASSIVE);
    if (menu) gtk_widget_destroy(menu);
    if (indicator) g_object_unref(indicator);
    indicator = NULL;
    menu = NULL;
}

#endif
