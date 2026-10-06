#include "tracker.h"

#include "afk.h"
#include "idle.h"
#include "window.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

double tracker_now_seconds(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value = { .LowPart = ft.dwLowDateTime, .HighPart = ft.dwHighDateTime };
    return (double)(value.QuadPart - 116444736000000000ULL) / 10000000.0;
#else
    struct timespec value;
    clock_gettime(CLOCK_REALTIME, &value);
    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
#endif
}

void tracker_sleep_seconds(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000.0));
#else
    usleep((useconds_t)(seconds * 1000000.0));
#endif
}

static int get_hostname(char *buffer, size_t size) {
#ifdef _WIN32
    DWORD length = (DWORD)size;
    return GetComputerNameA(buffer, &length) ? 0 : -1;
#else
    return gethostname(buffer, size) == 0 ? 0 : -1;
#endif
}

static int report_send(tracker_session *session, const char *event, int result) {
    if (session->options.on_send)
        session->options.on_send(session->options.callback_context, event, result);
    return result;
}

int tracker_session_init(tracker_session *session, http_client *client,
                         const char *email, const tracker_options *options) {
    memset(session, 0, sizeof(*session));
    session->client = client;
    session->options = *options;

    if (get_hostname(session->hostname, sizeof(session->hostname) - 1))
        strcpy(session->hostname, "unknown");

    if (email && *email) {
        size_t at = strcspn(email, "@");
        snprintf(session->username, sizeof(session->username), "%.*s", (int)at, email);
    }
    if (!session->username[0])
        snprintf(session->username, sizeof(session->username), "%s", session->hostname);

    snprintf(session->afk_bucket, sizeof(session->afk_bucket),
             "aw-watcher-afk_%s", session->username);
    snprintf(session->window_bucket, sizeof(session->window_bucket),
             "aw-watcher-window_%s", session->username);

    if (idle_init()) return -1;
    session->idle_available = true;

    /* Window tracking remains disabled until its API is implemented. */
    session->window_available = false;
    int result = http_create_bucket(client, session->afk_bucket,
                                    "aw-watcher-afk", "afkstatus", session->hostname);
    if (result) {
        report_send(session, "create AFK bucket", result);
        if (result == HTTP_RESULT_UNAUTHORIZED) {
            tracker_session_cleanup(session);
            return result;
        }
    }
    if (session->window_available &&
        (result = http_create_bucket(client, session->window_bucket,
                                     "aw-watcher-window", "currentwindow",
                                     session->hostname)) != HTTP_RESULT_OK) {
        report_send(session, "create foreground-process bucket", result);
        if (result == HTTP_RESULT_UNAUTHORIZED) {
            tracker_session_cleanup(session);
            return result;
        }
    }

    session->next_afk = tracker_now_seconds();
    session->next_window = session->next_afk;
    return 0;
}

int tracker_session_poll(tracker_session *session) {
    double now = tracker_now_seconds();

    if (session->window_available && now >= session->next_window) {
        window_info foreground;
        if (window_get_current(&foreground)) {
            strcpy(foreground.app, "unknown");
            foreground.title[0] = '\0';
        }
        if (session->options.exclude_window_title) strcpy(foreground.title, "excluded");
        char timestamp[32];
        if (!afk_format_timestamp(timestamp, sizeof(timestamp), now)) {
            int result = http_heartbeat_window(
                session->client, session->window_bucket, timestamp,
                foreground.app, foreground.title, session->options.window_poll_seconds + 1.0);
            if (report_send(session, "foreground-process heartbeat", result)
                == HTTP_RESULT_UNAUTHORIZED) return HTTP_RESULT_UNAUTHORIZED;
        }
        session->next_window = now + session->options.window_poll_seconds;
    }

    if (now >= session->next_afk) {
        double idle = idle_seconds();
        if (idle >= 0) {
            afk_sample sample = afk_update(
                session->afk, idle, session->options.afk_timeout_seconds);
            double last_input = now + sample.event_offset;
            double pulse_time = session->options.afk_timeout_seconds
                + session->options.afk_poll_seconds;
            char timestamp[32];
            if (sample.changed) {
                if (!afk_format_timestamp(timestamp, sizeof(timestamp), last_input)) {
                    int result = report_send(session, "afk heartbeat", http_heartbeat(
                        session->client, session->afk_bucket, timestamp, 0,
                        session->afk, pulse_time));
                    if (result == HTTP_RESULT_UNAUTHORIZED) return result;
                }
                if (!afk_format_timestamp(timestamp, sizeof(timestamp), last_input + 0.001)) {
                    int result = report_send(session, "afk heartbeat", http_heartbeat(
                        session->client, session->afk_bucket, timestamp, sample.duration,
                        sample.afk, pulse_time));
                    if (result == HTTP_RESULT_UNAUTHORIZED) return result;
                }
            } else if (!afk_format_timestamp(timestamp, sizeof(timestamp), last_input)) {
                int result = report_send(session, "afk heartbeat", http_heartbeat(
                    session->client, session->afk_bucket, timestamp, sample.duration,
                    sample.afk, pulse_time));
                if (result == HTTP_RESULT_UNAUTHORIZED) return result;
            }
            session->afk = sample.afk;
        }
        session->next_afk = now + session->options.afk_poll_seconds;
    }
    return HTTP_RESULT_OK;
}

double tracker_session_delay(const tracker_session *session) {
    double next = session->next_afk;
    if (session->window_available && session->next_window < next) next = session->next_window;
    double delay = next - tracker_now_seconds();
    return delay < 0.01 ? 0.01 : delay;
}

void tracker_session_cleanup(tracker_session *session) {
    if (session->window_available) window_cleanup();
    if (session->idle_available) idle_cleanup();
    session->window_available = false;
    session->idle_available = false;
}
