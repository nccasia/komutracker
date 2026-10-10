#include "auth.h"
#include "config.h"
#include "http.h"
#include "instance.h"
#include "tracker.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static volatile sig_atomic_t running = 1;
static void stop(int signal_number) { (void)signal_number; running = 0; }
static bool verbose_logging;

static void usage(const char *program) {
    fprintf(stderr,
        "Usage: %s [--login|--logout|--status] [--no-browser] [--testing] [-v]\n"
        "  [--server URL] [--token TOKEN] [--device-id ID] [--timeout SEC]\n"
        "  [--poll-time SEC] [--window-poll-time SEC] [--exclude-title]\n"
        "  [--auth-url URL] [--client-id ID]\n"
        "  [--redirect-uri URL] [--auth-timeout SEC] [--daemon] [--version]\n", program);
}

static int now_local(char *buffer, size_t size) {
    time_t whole = time(NULL);
    struct tm local;
#ifdef _WIN32
    if (localtime_s(&local, &whole) != 0) return -1;
#else
    if (localtime_r(&whole, &local) == NULL) return -1;
#endif
    return snprintf(buffer, size, "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec) >= (int)size ? -1 : 0;
}

static void log_send(void *context, const char *event, int result) {
    (void)context;
    if (!verbose_logging) return;
    char stamp[16];
    if (now_local(stamp, sizeof(stamp)) == 0)
        fprintf(stderr, "komutracker %s [%s] %s: %s\n", KOMUTRACKER_VERSION, stamp, event,
                result == 0 ? "OK" : "FAILED");
}

static void log_info(const char *message) {
    if (!verbose_logging) return;
    char stamp[16];
    if (now_local(stamp, sizeof(stamp)) == 0)
        fprintf(stderr, "komutracker %s [%s] %s\n", KOMUTRACKER_VERSION, stamp, message);
}

int main(int argc, char **argv) {
    bool testing = false, verbose = false, login = false, logout = false, status = false, no_browser = false;
    bool exclude_title = false, version = false, daemon = false;
    komutracker_config config = KOMUTRACKER_CONFIG;
    const char *token_arg = NULL, *device_arg = NULL;
    bool server_overridden = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--testing")) testing = true;
        else if (!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v")) verbose = true;
        else if (!strcmp(argv[i], "--login")) login = true;
        else if (!strcmp(argv[i], "--logout")) logout = true;
        else if (!strcmp(argv[i], "--status")) status = true;
        else if (!strcmp(argv[i], "--no-browser")) no_browser = true;
        else if (!strcmp(argv[i], "--exclude-title")) exclude_title = true;
        else if (!strcmp(argv[i], "--daemon") || !strcmp(argv[i], "-d")) daemon = true;
        else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V")) version = true;
        else if (!strcmp(argv[i], "--timeout") && i + 1 < argc) config.afk_timeout_seconds = strtod(argv[++i], NULL);
        else if (!strcmp(argv[i], "--poll-time") && i + 1 < argc) config.afk_poll_seconds = strtod(argv[++i], NULL);
        else if (!strcmp(argv[i], "--window-poll-time") && i + 1 < argc) config.window_poll_seconds = strtod(argv[++i], NULL);
        else if (!strcmp(argv[i], "--auth-timeout") && i + 1 < argc) config.auth_timeout_seconds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--server") && i + 1 < argc) {
            config.server_url = argv[++i];
            server_overridden = true;
        }
        else if (!strcmp(argv[i], "--token") && i + 1 < argc) token_arg = argv[++i];
        else if (!strcmp(argv[i], "--device-id") && i + 1 < argc) device_arg = argv[++i];
        else if (!strcmp(argv[i], "--auth-url") && i + 1 < argc) config.auth_url = argv[++i];
        else if (!strcmp(argv[i], "--client-id") && i + 1 < argc) config.oauth_client_id = argv[++i];
        else if (!strcmp(argv[i], "--redirect-uri") && i + 1 < argc) config.oauth_redirect_uri = argv[++i];
        else { usage(argv[0]); return 2; }
    }
    if ((login ? 1 : 0) + (logout ? 1 : 0) + (status ? 1 : 0) > 1) { usage(argv[0]); return 2; }
    if (version) { printf("komutracker %s\n", KOMUTRACKER_VERSION); return 0; }
    verbose_logging = verbose;
    if (testing) {
        config.afk_timeout_seconds = KOMUTRACKER_TEST_CONFIG.afk_timeout_seconds;
        config.afk_poll_seconds = KOMUTRACKER_TEST_CONFIG.afk_poll_seconds;
        config.window_poll_seconds = KOMUTRACKER_TEST_CONFIG.window_poll_seconds;
        if (!server_overridden) config.server_url = KOMUTRACKER_TEST_CONFIG.server_url;
    }
    if (config.afk_timeout_seconds <= 0 || config.afk_poll_seconds <= 0 ||
        config.window_poll_seconds <= 0 ||
        config.afk_timeout_seconds < config.afk_poll_seconds ||
        config.auth_timeout_seconds <= 0) return 2;

    signal(SIGINT, stop); signal(SIGTERM, stop);
    http_set_running_flag(&running);
    if (http_global_init()) return 1;

    char device[256], saved_token[8192] = {0};
    if (device_arg) snprintf(device, sizeof(device), "%s", device_arg);
    else if (auth_get_device_id(device, sizeof(device))) {
        fprintf(stderr, "Cannot load device ID\n");
        http_global_cleanup();
        return 1;
    }
    const char *token = token_arg;
    if (!token && auth_read_token(saved_token, sizeof(saved_token)) == 0) token = saved_token;
    http_client client = { config.server_url, token, device, verbose };
    auth_options options = {
        config.auth_url,
        config.oauth_client_id,
        config.oauth_redirect_uri,
        config.auth_timeout_seconds,
        !no_browser,
    };

    if (logout) {
        int remote = http_auth_delete(&client);
        auth_remove_token();
        auth_remove_profile();
        http_global_cleanup();
        if (remote) { fprintf(stderr, "Local token removed; server logout failed\n"); return 1; }
        printf("Logged out\n"); return 0;
    }

    int verification = token ? http_auth_me(&client, NULL, 0, NULL, 0) : HTTP_AUTH_UNAUTHORIZED;
    if (login || verification == HTTP_AUTH_UNAUTHORIZED) {
        if (token && verification == HTTP_AUTH_UNAUTHORIZED)
            tracker_discard_pending_events(&client);
        if (!token_arg) auth_remove_token();
        if (auth_login(&client, &options, device, sizeof(device),
                       saved_token, sizeof(saved_token), &running)) {
            http_global_cleanup(); return running ? 1 : 130;
        }
    } else if (verification != HTTP_AUTH_OK && verbose) {
        log_info("server unavailable; starting with cached authentication");
    }

    if (status || login) {
        char name[512], email[512];
        int result = http_auth_me(&client, name, sizeof(name), email, sizeof(email));
        if (result == HTTP_AUTH_OK) auth_save_profile(name, email);
        http_global_cleanup();
        if (result != HTTP_AUTH_OK) return 1;
        printf("Authenticated as %s <%s>\n", name, email);
        if (status || login) return 0;
    }

    /* Once committed to tracking, ensure only one instance runs. Acquire the
       lock before detaching so a duplicate launch is reported to the terminal. */
    int lock_status = instance_lock();
    if (lock_status != 0) {
        if (lock_status < 0) {
            fprintf(stderr, "Failed to acquire instance lock\n");
        } else if (verbose) {
            fprintf(stderr, "Another komutracker instance is already running\n");
        }
        http_global_cleanup();
        return 1;
    }

    if (daemon) {
        if (instance_daemonize() != 0) {
            fprintf(stderr, "Failed to run as daemon\n");
            http_global_cleanup();
            return 1;
        }
    }

    char name[512] = {0}, email[512] = {0};
    auth_read_profile(name, sizeof(name), email, sizeof(email));
    int profile = http_auth_me(&client, name, sizeof(name), email, sizeof(email));
    if (profile == HTTP_AUTH_OK) {
        auth_save_profile(name, email);
        if (verbose) {
            char message[1080];
            snprintf(message, sizeof(message), "logged in as %s <%s>", name, email);
            log_info(message);
        }
    } else if (verbose) {
        log_info(email[0]
            ? "logged-in user unavailable; using cached profile"
            : "logged-in user unavailable; using hostname for bucket name");
    }

    tracker_options tracker_config = {
        .afk_timeout_seconds = config.afk_timeout_seconds,
        .afk_poll_seconds = config.afk_poll_seconds,
        .window_poll_seconds = config.window_poll_seconds,
        .exclude_window_title = exclude_title,
        .on_send = log_send,
    };
    tracker_session tracker;
    if (tracker_session_init(&tracker, &client, email, &tracker_config)) {
        http_global_cleanup();
        return 1;
    }
    if (verbose) {
        char message[1200];
        snprintf(message, sizeof(message), "afk bucket: %s", tracker.afk_bucket);
        log_info(message);
        snprintf(message, sizeof(message), "foreground-process bucket: %s", tracker.window_bucket);
        log_info(message);
        if (!tracker.window_available)
            log_info("foreground process tracking is unavailable");
    }

    if (verbose_logging)
        fprintf(stderr, "komutracker %s started for %s\n",
                KOMUTRACKER_VERSION, config.server_url);
    while (running) {
        if (tracker_session_poll(&tracker) == HTTP_RESULT_UNAUTHORIZED) break;
        tracker_sleep_seconds(tracker_session_delay(&tracker));
    }
    tracker_session_cleanup(&tracker);
    http_global_cleanup();
    return 0;
}
