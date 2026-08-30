#include "afk.h"
#include "auth.h"
#include "http.h"
#include "idle.h"
#include "window.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#define sleep_seconds(v) Sleep((DWORD)((v) * 1000.0))
#else
#include <unistd.h>
#define sleep_seconds(v) usleep((useconds_t)((v) * 1000000.0))
#endif

static volatile sig_atomic_t running = 1;
static void stop(int signal_number) { (void)signal_number; running = 0; }

static void usage(const char *program) {
    fprintf(stderr,
        "Usage: %s [--login|--logout|--status] [--no-browser] [--testing] [-v]\n"
        "  [--server URL] [--token TOKEN] [--device-id ID] [--timeout SEC]\n"
        "  [--poll-time SEC] [--window-poll-time SEC] [--exclude-title]\n"
        "  [--auth-url URL] [--client-id ID]\n"
        "  [--redirect-uri URL] [--auth-timeout SEC] [--version]\n", program);
}

static double now_seconds(void) {
#ifdef _WIN32
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value = { .LowPart = ft.dwLowDateTime, .HighPart = ft.dwHighDateTime };
    return (double)(value.QuadPart - 116444736000000000ULL) / 10000000.0;
#else
    struct timespec value; clock_gettime(CLOCK_REALTIME, &value);
    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
#endif
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

static int get_hostname(char *buffer, size_t size) {
#ifdef _WIN32
    DWORD length = (DWORD)size; return GetComputerNameA(buffer, &length) ? 0 : -1;
#else
    return gethostname(buffer, size) == 0 ? 0 : -1;
#endif
}

static void log_send(const char *event, int result) {
    char stamp[16];
    if (now_local(stamp, sizeof(stamp)) == 0)
        fprintf(stderr, "komutracker %s [%s] %s: %s\n", KOMUTRACKER_VERSION, stamp, event,
                result == 0 ? "OK" : "FAILED");
}

static void log_info(const char *message) {
    char stamp[16];
    if (now_local(stamp, sizeof(stamp)) == 0)
        fprintf(stderr, "komutracker %s [%s] %s\n", KOMUTRACKER_VERSION, stamp, message);
}

int main(int argc, char **argv) {
    bool testing = false, verbose = false, login = false, logout = false, status = false, no_browser = false;
    bool exclude_title = false, version = false;
    double timeout = 180.0, poll_time = 5.0, window_poll_time = 10.0;
    int auth_timeout = getenv("AW_AUTH_TIMEOUT") ? atoi(getenv("AW_AUTH_TIMEOUT")) : 300;
    const char *server = getenv("AW_SERVER_URL"), *token_arg = getenv("AW_AUTH_TOKEN");
    const char *device_arg = getenv("AW_DEVICE_ID");
    const char *auth_url = getenv("AW_AUTH_URL");
    const char *client_id = getenv("AW_CLIENT_ID");
    const char *redirect_uri = getenv("AW_REDIRECT_URI");

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--testing")) testing = true;
        else if (!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v")) verbose = true;
        else if (!strcmp(argv[i], "--login")) login = true;
        else if (!strcmp(argv[i], "--logout")) logout = true;
        else if (!strcmp(argv[i], "--status")) status = true;
        else if (!strcmp(argv[i], "--no-browser")) no_browser = true;
        else if (!strcmp(argv[i], "--exclude-title")) exclude_title = true;
        else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V")) version = true;
        else if (!strcmp(argv[i], "--timeout") && i + 1 < argc) timeout = strtod(argv[++i], NULL);
        else if (!strcmp(argv[i], "--poll-time") && i + 1 < argc) poll_time = strtod(argv[++i], NULL);
        else if (!strcmp(argv[i], "--window-poll-time") && i + 1 < argc) window_poll_time = strtod(argv[++i], NULL);
        else if (!strcmp(argv[i], "--auth-timeout") && i + 1 < argc) auth_timeout = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--server") && i + 1 < argc) server = argv[++i];
        else if (!strcmp(argv[i], "--token") && i + 1 < argc) token_arg = argv[++i];
        else if (!strcmp(argv[i], "--device-id") && i + 1 < argc) device_arg = argv[++i];
        else if (!strcmp(argv[i], "--auth-url") && i + 1 < argc) auth_url = argv[++i];
        else if (!strcmp(argv[i], "--client-id") && i + 1 < argc) client_id = argv[++i];
        else if (!strcmp(argv[i], "--redirect-uri") && i + 1 < argc) redirect_uri = argv[++i];
        else { usage(argv[0]); return 2; }
    }
    if ((login ? 1 : 0) + (logout ? 1 : 0) + (status ? 1 : 0) > 1) { usage(argv[0]); return 2; }
    if (version) { printf("komutracker %s\n", KOMUTRACKER_VERSION); return 0; }
    if (testing) { timeout = 20.0; poll_time = 1.0; }
    if (!server) server = testing ? "http://127.0.0.1:5666" : "https://tracker-api.komu.vn";
    if (!auth_url) auth_url = "https://oauth2.mezon.ai";
    if (!client_id) client_id = "1840672452439445504";
    if (!redirect_uri) redirect_uri = "https://tracker-api.komu.vn/api/0/auth/callback";
    if (timeout <= 0 || poll_time <= 0 || window_poll_time <= 0 || timeout < poll_time || auth_timeout <= 0) return 2;

    signal(SIGINT, stop); signal(SIGTERM, stop);
    http_set_running_flag(&running);
    if (http_global_init()) return 1;

    char device[256], saved_token[8192] = {0};
    if (device_arg) snprintf(device, sizeof(device), "%s", device_arg);
    else if (auth_get_device_id(device, sizeof(device))) { fprintf(stderr, "Cannot load device ID\n"); return 1; }
    const char *token = token_arg;
    if (!token && auth_read_token(saved_token, sizeof(saved_token)) == 0) token = saved_token;
    http_client client = { server, token, device, verbose };
    auth_options options = { auth_url, client_id, redirect_uri, auth_timeout, !no_browser };

    if (logout) {
        int remote = http_auth_delete(&client);
        auth_remove_token();
        http_global_cleanup();
        if (remote) { fprintf(stderr, "Local token removed; server logout failed\n"); return 1; }
        printf("Logged out\n"); return 0;
    }

    int verification = token ? http_auth_me(&client, NULL, 0, NULL, 0) : HTTP_AUTH_UNAUTHORIZED;
    if (login || verification == HTTP_AUTH_UNAUTHORIZED) {
        if (!token_arg) auth_remove_token();
        if (auth_login(&client, &options, saved_token, sizeof(saved_token), &running)) {
            http_global_cleanup(); return running ? 1 : 130;
        }
        token = saved_token;
    } else if (verification != HTTP_AUTH_OK) {
        fprintf(stderr, "Unable to verify authentication because the server is unavailable\n");
        http_global_cleanup(); return 1;
    }

    if (status || login) {
        char name[512], email[512];
        int result = http_auth_me(&client, name, sizeof(name), email, sizeof(email));
        http_global_cleanup();
        if (result != HTTP_AUTH_OK) return 1;
        printf("Authenticated as %s <%s>\n", name, email);
        if (status || login) return 0;
    }

    char host[256] = {0}; if (get_hostname(host, sizeof(host) - 1)) strcpy(host, "unknown");
    char name[512] = {0}, email[512] = {0};
    int profile = http_auth_me(&client, name, sizeof(name), email, sizeof(email));
    char username[512] = {0};
    if (profile == HTTP_AUTH_OK) {
        size_t at = strcspn(email, "@");
        snprintf(username, sizeof(username), "%.*s", (int)at, email);
        if (verbose) {
            char message[1080];
            snprintf(message, sizeof(message), "logged in as %s <%s>", name, email);
            log_info(message);
        }
    } else {
        strcpy(username, host);
        if (verbose) log_info("logged-in user unavailable; using hostname for bucket name");
    }
    char afk_bucket[512], window_bucket[512];
    snprintf(afk_bucket, sizeof(afk_bucket), "aw-watcher-afk_%s", username);
    snprintf(window_bucket, sizeof(window_bucket), "aw-watcher-window_%s", username);
    if (verbose) {
        char message[1080];
        snprintf(message, sizeof(message), "afk bucket: %s", afk_bucket);
        log_info(message);
        snprintf(message, sizeof(message), "foreground-process bucket: %s", window_bucket);
        log_info(message);
    }
    if (idle_init()) { http_global_cleanup(); return 1; }
    bool window_available = window_init() == 0;
    if (!window_available) fprintf(stderr, "Foreground process tracking is unavailable\n");
    if (http_create_bucket(&client, afk_bucket, "aw-watcher-afk", "afkstatus", host))
        fprintf(stderr, "Unable to create AFK bucket\n");
    if (window_available && http_create_bucket(&client, window_bucket, "aw-watcher-window", "currentwindow", host))
        fprintf(stderr, "Unable to create foreground-process bucket\n");

    bool afk = false;
    double next_afk = now_seconds(), next_window = next_afk;
    fprintf(stderr, "komutracker %s started for %s\n", KOMUTRACKER_VERSION, server);
    while (running) {
        double now = now_seconds();
        if (window_available && now >= next_window) {
            window_info foreground;
            if (window_get_current(&foreground)) {
                strcpy(foreground.app, "unknown"); foreground.title[0] = '\0';
            }
            if (exclude_title) strcpy(foreground.title, "excluded");
            char timestamp[32];
            if (!afk_format_timestamp(timestamp, sizeof(timestamp), now)) {
                int result = http_heartbeat_window(&client, window_bucket, timestamp, foreground.app,
                                                   foreground.title, window_poll_time + 1.0);
                log_send("foreground-process heartbeat", result);
            }
            next_window = now + window_poll_time;
        }

        if (now >= next_afk) {
            double idle = idle_seconds();
            if (idle >= 0) {
                afk_sample sample = afk_update(afk, idle, timeout);
                double last_input = now + sample.event_offset;
                char timestamp[32];
                if (sample.changed) {
                    if (!afk_format_timestamp(timestamp, sizeof(timestamp), last_input))
                        log_send("afk heartbeat", http_heartbeat(&client, afk_bucket, timestamp, 0, afk, timeout + poll_time));
                    if (!afk_format_timestamp(timestamp, sizeof(timestamp), last_input + 0.001))
                        log_send("afk heartbeat", http_heartbeat(&client, afk_bucket, timestamp, sample.duration, sample.afk, timeout + poll_time));
                } else if (!afk_format_timestamp(timestamp, sizeof(timestamp), last_input))
                    log_send("afk heartbeat", http_heartbeat(&client, afk_bucket, timestamp, sample.duration, sample.afk, timeout + poll_time));
                afk = sample.afk;
            }
            next_afk = now + poll_time;
        }

        double next = next_afk;
        if (window_available && next_window < next) next = next_window;
        double delay = next - now_seconds();
        if (delay < 0.01) delay = 0.01;
        sleep_seconds(delay);
    }
    if (window_available) window_cleanup();
    idle_cleanup(); http_global_cleanup();
    return 0;
}
