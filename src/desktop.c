#include "desktop.h"

#include "auth.h"
#include "config.h"
#include "http.h"
#include "instance.h"
#include "tracker.h"
#include "tray.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE worker_thread;
typedef CRITICAL_SECTION app_mutex;
#else
#include <pthread.h>
typedef pthread_t worker_thread;
typedef pthread_mutex_t app_mutex;
#endif

typedef struct {
    volatile sig_atomic_t running;
    volatile sig_atomic_t login_running;
    app_mutex mutex;
    bool login_requested;
    bool logout_requested;
    bool dashboard_requested;
    tray_view view;
    const komutracker_config *config;
} desktop_app;

static desktop_app *signal_app;

static void mutex_init(app_mutex *mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
#else
    pthread_mutex_init(mutex, NULL);
#endif
}

static void mutex_lock(app_mutex *mutex) {
#ifdef _WIN32
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

static void mutex_unlock(app_mutex *mutex) {
#ifdef _WIN32
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

static void mutex_destroy(app_mutex *mutex) {
#ifdef _WIN32
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

static void set_view(desktop_app *app, tray_status status, bool logged_in,
                     const char *name, const char *status_text) {
    mutex_lock(&app->mutex);
    app->view.status = status;
    app->view.logged_in = logged_in;
    snprintf(app->view.account_name, sizeof(app->view.account_name), "%s",
             name && *name ? name : (logged_in ? "Signed In" : "Not Logged In"));
    snprintf(app->view.status_text, sizeof(app->view.status_text), "%s",
             status_text ? status_text : "");
    mutex_unlock(&app->mutex);
}

static bool take_flag(desktop_app *app, bool *flag) {
    mutex_lock(&app->mutex);
    bool value = *flag;
    *flag = false;
    mutex_unlock(&app->mutex);
    return value;
}

static bool is_logout_requested(desktop_app *app) {
    mutex_lock(&app->mutex);
    bool value = app->logout_requested;
    mutex_unlock(&app->mutex);
    return value;
}

static void open_dashboard(desktop_app *app, const char *username) {
    char url[2048];
    int count = snprintf(url, sizeof(url), "%s/?username=%s", app->config->server_url,
                         username ? username : "");
    if (count > 0 && count < (int)sizeof(url)) auth_open_browser(url);
}

static int run_tracking(desktop_app *app, http_client *client,
                        const char *name, const char *email) {
    tracker_options options = {
        .afk_timeout_seconds = app->config->afk_timeout_seconds,
        .afk_poll_seconds = app->config->afk_poll_seconds,
        .window_poll_seconds = app->config->window_poll_seconds,
    };
    tracker_session session;
    int initialization = tracker_session_init(&session, client, email, &options);
    if (initialization == HTTP_RESULT_UNAUTHORIZED) return HTTP_RESULT_UNAUTHORIZED;
    if (initialization != HTTP_RESULT_OK) {
        set_view(app, TRAY_CONNECTION_ERROR, true, name, "Idle detection unavailable");
        while (app->running && !is_logout_requested(app)) {
            if (take_flag(app, &app->dashboard_requested)) open_dashboard(app, "");
            tracker_sleep_seconds(0.1);
        }
        return app->running ? 1 : 0;
    }

    set_view(app, TRAY_TRACKING, true, name, "Tracking");

    while (app->running && !is_logout_requested(app)) {
        if (take_flag(app, &app->dashboard_requested))
            open_dashboard(app, session.username);
        if (tracker_session_poll(&session) == HTTP_RESULT_UNAUTHORIZED) {
            tracker_session_cleanup(&session);
            return HTTP_RESULT_UNAUTHORIZED;
        }
        tracker_sleep_seconds(0.1);
    }

    tracker_session_cleanup(&session);
    return app->running ? 1 : 0;
}

static void logout(desktop_app *app, http_client *client) {
    set_view(app, TRAY_CONNECTING, true, NULL, "Logging out…");
    http_auth_delete(client);
    auth_remove_token();
    auth_remove_profile();
    mutex_lock(&app->mutex);
    app->logout_requested = false;
    mutex_unlock(&app->mutex);
    set_view(app, TRAY_LOGGED_OUT, false, NULL, "Tracking stopped");
}

static void desktop_worker(desktop_app *app) {
    if (http_global_init()) {
        set_view(app, TRAY_CONNECTION_ERROR, false, NULL, "Unable to initialize networking");
        return;
    }
    http_set_running_flag(&app->running);

    char device[256];
    if (auth_get_device_id(device, sizeof(device))) {
        set_view(app, TRAY_CONNECTION_ERROR, false, NULL, "Unable to create device identity");
        http_global_cleanup();
        return;
    }

    char token[8192] = {0};
    bool has_token = auth_read_token(token, sizeof(token)) == 0;
    bool session_replaced = false;
    char cached_name[512] = {0}, cached_email[512] = {0};
    auth_read_profile(cached_name, sizeof(cached_name), cached_email, sizeof(cached_email));

    http_client client = {
        app->config->server_url, has_token ? token : NULL, device, false,
    };
    auth_options options = {
        app->config->auth_url,
        app->config->oauth_client_id,
        app->config->oauth_redirect_uri,
        app->config->auth_timeout_seconds,
        true,
    };

    while (app->running) {
        if (!has_token) {
            set_view(app, TRAY_LOGGED_OUT, false, NULL,
                     session_replaced ? "Session replaced by another login" : "Tracking stopped");
            while (app->running && !take_flag(app, &app->login_requested)) {
                take_flag(app, &app->dashboard_requested);
                tracker_sleep_seconds(0.1);
            }
            if (!app->running) break;
            session_replaced = false;

            set_view(app, TRAY_AUTHENTICATING, false, NULL, "Waiting for browser authentication");
            app->login_running = 1;
            memset(token, 0, sizeof(token));
            client.token = NULL;
            if (auth_login(&client, &options, device, sizeof(device),
                           token, sizeof(token), &app->login_running) != 0) {
                if (!app->running) break;
                set_view(app, TRAY_LOGGED_OUT, false, NULL,
                         app->login_running ? "Login failed — try again" : "Login cancelled");
                app->login_running = 0;
                continue;
            }
            app->login_running = 0;
            has_token = true;
            client.token = token;
        }

        set_view(app, TRAY_CONNECTING, true, cached_name, "Connecting…");
        char name[512] = {0}, email[512] = {0};
        int profile = http_auth_me(&client, name, sizeof(name), email, sizeof(email));
        if (profile == HTTP_AUTH_UNAUTHORIZED) {
            auth_remove_token();
            auth_remove_profile();
            memset(token, 0, sizeof(token));
            client.token = NULL;
            has_token = false;
            session_replaced = true;
            cached_name[0] = cached_email[0] = '\0';
            continue;
        }
        if (profile != HTTP_AUTH_OK) {
            set_view(app, TRAY_CONNECTION_ERROR, true, cached_name, "Connection unavailable");
            for (int tick = 0; tick < 100 && app->running && !is_logout_requested(app); tick++) {
                if (take_flag(app, &app->dashboard_requested)) open_dashboard(app, "");
                tracker_sleep_seconds(0.1);
            }
            if (is_logout_requested(app)) {
                logout(app, &client);
                memset(token, 0, sizeof(token));
                client.token = NULL;
                has_token = false;
                cached_name[0] = cached_email[0] = '\0';
            }
            continue;
        }

        snprintf(cached_name, sizeof(cached_name), "%s", name);
        snprintf(cached_email, sizeof(cached_email), "%s", email);
        auth_save_profile(name, email);
        int tracking_result = run_tracking(app, &client, name, email);

        if (tracking_result == HTTP_RESULT_UNAUTHORIZED) {
            auth_remove_token();
            auth_remove_profile();
            memset(token, 0, sizeof(token));
            client.token = NULL;
            has_token = false;
            session_replaced = true;
            cached_name[0] = cached_email[0] = '\0';
            set_view(app, TRAY_LOGGED_OUT, false, NULL,
                     "Session replaced by another login");
            continue;
        }

        if (app->running && is_logout_requested(app)) {
            logout(app, &client);
            memset(token, 0, sizeof(token));
            client.token = NULL;
            has_token = false;
            cached_name[0] = cached_email[0] = '\0';
        }
    }

    app->login_running = 0;
    http_global_cleanup();
}

#ifdef _WIN32
static DWORD WINAPI worker_entry(LPVOID context) {
    desktop_worker(context);
    return 0;
}

static int worker_start(worker_thread *thread, desktop_app *app) {
    *thread = CreateThread(NULL, 0, worker_entry, app, 0, NULL);
    return *thread ? 0 : -1;
}

static void worker_join(worker_thread thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}
#else
static void *worker_entry(void *context) {
    desktop_worker(context);
    return NULL;
}

static int worker_start(worker_thread *thread, desktop_app *app) {
    return pthread_create(thread, NULL, worker_entry, app);
}

static void worker_join(worker_thread thread) {
    pthread_join(thread, NULL);
}
#endif

static void auth_action(void *context) {
    desktop_app *app = context;
    mutex_lock(&app->mutex);
    if (app->view.status == TRAY_AUTHENTICATING) app->login_running = 0;
    else app->login_requested = true;
    mutex_unlock(&app->mutex);
}

static void request_logout(void *context) {
    desktop_app *app = context;
    mutex_lock(&app->mutex);
    app->logout_requested = true;
    mutex_unlock(&app->mutex);
}

static void request_dashboard(void *context) {
    desktop_app *app = context;
    mutex_lock(&app->mutex);
    app->dashboard_requested = true;
    mutex_unlock(&app->mutex);
}

static void request_quit(void *context) {
    desktop_app *app = context;
    app->running = 0;
    app->login_running = 0;
}

static void signal_stop(int signal_number) {
    (void)signal_number;
    if (signal_app) {
        signal_app->running = 0;
        signal_app->login_running = 0;
    }
}

int komutracker_desktop_main(void) {
    int lock = instance_lock();
    if (lock != 0) return lock < 0 ? 1 : 0;

    desktop_app app;
    memset(&app, 0, sizeof(app));
    app.running = 1;
    app.config = &KOMUTRACKER_CONFIG;
    mutex_init(&app.mutex);
    set_view(&app, TRAY_CONNECTING, false, NULL, "Starting…");

    tray_callbacks callbacks = {
        auth_action, request_logout, request_dashboard, request_quit, &app,
    };
    if (tray_init(&callbacks)) {
        mutex_destroy(&app.mutex);
        return 1;
    }

    signal_app = &app;
    signal(SIGINT, signal_stop);
    signal(SIGTERM, signal_stop);

    worker_thread worker;
    if (worker_start(&worker, &app)) {
        tray_cleanup();
        mutex_destroy(&app.mutex);
        return 1;
    }

    while (app.running) {
        tray_view snapshot;
        mutex_lock(&app.mutex);
        snapshot = app.view;
        mutex_unlock(&app.mutex);
        tray_update(&snapshot);
        tray_poll();
        tracker_sleep_seconds(0.05);
    }

    app.login_running = 0;
    worker_join(worker);
    tray_cleanup();
    signal_app = NULL;
    mutex_destroy(&app.mutex);
    return 0;
}
