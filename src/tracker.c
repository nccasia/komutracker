#include "tracker.h"

#include "afk.h"
#include "dirs.h"
#include "idle.h"
#include "window.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define EVENT_RETENTION_SECONDS (5 * 24 * 60 * 60)
#define EVENT_FLUSH_BATCH 100
#define EVENT_RETRY_SECONDS 10.0
#define EVENT_MERGE_WINDOW_SECONDS 370.0
#define EVENT_CHECKPOINT_SECONDS 300.0

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

static int ensure_afk_bucket(tracker_session *session) {
    if (session->afk_bucket_available) return HTTP_RESULT_OK;
    int result = http_create_bucket(session->client, session->afk_bucket,
                                    "aw-watcher-afk", "afkstatus", session->hostname);
    report_send(session, "create AFK bucket", result);
    if (result == HTTP_RESULT_OK) session->afk_bucket_available = true;
    return result;
}

static int flush_pending_events(tracker_session *session) {
    int result = ensure_afk_bucket(session);
    if (result != HTTP_RESULT_OK) return result;
    if (!session->event_store_has_pending) return HTTP_RESULT_OK;

    for (int sent = 0; sent < EVENT_FLUSH_BATCH; sent++) {
        stored_event event;
        int found = event_store_next_pending(
            &session->events, session->client->base_url,
            session->client->device_id, &event);
        if (found < 0) return HTTP_RESULT_ERROR;
        if (found == 0) {
            session->event_store_has_pending = false;
            return HTTP_RESULT_OK;
        }

        result = http_heartbeat_json(
            session->client, event.bucket, event.timestamp, event.duration,
            event.data_json, event.pulsetime);
        report_send(session, "afk heartbeat", result);
        if (result != HTTP_RESULT_OK) return result;
        if (event_store_mark_sent(&session->events, event.id, (long long)time(NULL)))
            return HTTP_RESULT_ERROR;
    }
    return HTTP_RESULT_OK;
}

static int build_afk_event(tracker_session *session, stored_event *event,
                           const char *timestamp, double duration, bool afk,
                           double pulsetime) {
    memset(event, 0, sizeof(*event));
    int invalid = false ||
        snprintf(event->server_url, sizeof(event->server_url), "%s", session->client->base_url) >= (int)sizeof(event->server_url) ||
        snprintf(event->device_id, sizeof(event->device_id), "%s", session->client->device_id) >= (int)sizeof(event->device_id) ||
        snprintf(event->bucket, sizeof(event->bucket), "%s", session->afk_bucket) >= (int)sizeof(event->bucket) ||
        snprintf(event->timestamp, sizeof(event->timestamp), "%s", timestamp) >= (int)sizeof(event->timestamp);
    if (invalid) return -1;
    strcpy(event->kind, "afkstatus");
    strcpy(event->data_json, afk
        ? "{\"status\":\"afk\"}" : "{\"status\":\"not-afk\"}");
    event->duration = duration;
    event->pulsetime = pulsetime;
    event->created_at = (long long)time(NULL);
    return 0;
}

static int checkpoint_buffered_event(tracker_session *session) {
    if (!session->buffered_event_available || !session->buffered_event_dirty)
        return 0;
    if (event_store_merge(&session->events, &session->buffered_event,
                          EVENT_MERGE_WINDOW_SECONDS, NULL))
        return -1;
    session->buffered_event_dirty = false;
    session->buffered_event_checkpoint_end = session->buffered_event_end;
    session->event_store_has_pending = true;
    session->tracked_today_cache_valid = false;
    return 0;
}

static void invalidate_tracking_session(tracker_session *session) {
    if (!session->event_store_available) return;
    if (checkpoint_buffered_event(session))
        report_send(session, "persist AFK heartbeat", HTTP_RESULT_ERROR);
    if (event_store_discard_pending(
            &session->events, session->client->base_url,
            session->client->device_id, (long long)time(NULL)))
        report_send(session, "discard invalid-session events", HTTP_RESULT_ERROR);
    session->buffered_event_available = false;
    session->buffered_event_dirty = false;
    session->event_store_has_pending = false;
    session->tracked_today_cache_valid = false;
}

static int buffer_afk_event(tracker_session *session, const char *timestamp,
                            double start, double duration, bool afk,
                            double pulsetime) {
    stored_event event;
    if (build_afk_event(session, &event, timestamp, duration, afk, pulsetime))
        return -1;

    double end = start + duration;
    bool compatible = session->buffered_event_available &&
        strcmp(session->buffered_event.data_json, event.data_json) == 0;
    if (compatible) {
        double gap = start > session->buffered_event_end
            ? start - session->buffered_event_end
            : session->buffered_event_start - end;
        compatible = gap <= EVENT_MERGE_WINDOW_SECONDS;
    }

    if (!compatible) {
        if (checkpoint_buffered_event(session)) return -1;
        session->buffered_event = event;
        session->buffered_event_available = true;
        session->buffered_event_start = start;
        session->buffered_event_end = end;
        session->buffered_event_checkpoint_end = start;
        session->buffered_event_dirty = true;
        return 0;
    }

    if (start < session->buffered_event_start) {
        session->buffered_event_start = start;
        snprintf(session->buffered_event.timestamp,
                 sizeof(session->buffered_event.timestamp), "%s", timestamp);
    }
    if (end > session->buffered_event_end)
        session->buffered_event_end = end;
    session->buffered_event.duration = session->buffered_event_end -
        session->buffered_event_start;
    if (pulsetime > session->buffered_event.pulsetime)
        session->buffered_event.pulsetime = pulsetime;
    session->buffered_event.created_at = event.created_at;
    session->buffered_event_dirty = true;
    return 0;
}

static int send_buffered_event(tracker_session *session) {
    if (!session->buffered_event_available || !session->buffered_event_dirty)
        return HTTP_RESULT_OK;
    stored_event *event = &session->buffered_event;
    int result = http_heartbeat_json(
        session->client, event->bucket, event->timestamp, event->duration,
        event->data_json, event->pulsetime);
    return report_send(session, "afk heartbeat", result);
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

    char event_store_path[2048];
    if (dirs_event_store_path(event_store_path, sizeof(event_store_path)) ||
        dirs_create_parent(event_store_path) ||
        event_store_open(&session->events, event_store_path)) {
        fprintf(stderr, "Cannot open the local event journal\n");
        return HTTP_RESULT_ERROR;
    }
    session->event_store_available = true;
    event_store_prune(&session->events,
                      (long long)time(NULL) - EVENT_RETENTION_SECONDS);
    int pending = event_store_pending_count(
        &session->events, session->client->base_url,
        session->client->device_id);
    if (pending < 0) {
        tracker_session_cleanup(session);
        return HTTP_RESULT_ERROR;
    }
    session->event_store_has_pending = pending > 0;
    session->next_event_prune = tracker_now_seconds() + 3600.0;

    if (idle_init()) {
        tracker_session_cleanup(session);
        return HTTP_RESULT_ERROR;
    }
    session->idle_available = true;

    /* Window tracking remains disabled until its API is implemented. */
    session->window_available = false;
    int result = ensure_afk_bucket(session);
    if (result != HTTP_RESULT_OK) {
        if (result == HTTP_RESULT_UNAUTHORIZED) {
            invalidate_tracking_session(session);
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
            invalidate_tracking_session(session);
            tracker_session_cleanup(session);
            return result;
        }
    }

    session->next_afk = tracker_now_seconds();
    session->next_window = session->next_afk;
    session->next_event_flush = session->next_afk +
        (session->afk_bucket_available ? 0.0 : EVENT_RETRY_SECONDS);
    session->next_event_checkpoint = session->next_afk + EVENT_CHECKPOINT_SECONDS;
    return 0;
}

int tracker_session_poll(tracker_session *session) {
    double now = tracker_now_seconds();

    if (now >= session->next_event_prune) {
        event_store_prune(&session->events,
                          (long long)time(NULL) - EVENT_RETENTION_SECONDS);
        session->next_event_prune = now + 3600.0;
    }

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
                == HTTP_RESULT_UNAUTHORIZED) {
                invalidate_tracking_session(session);
                return HTTP_RESULT_UNAUTHORIZED;
            }
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
            bool buffered = true;
            if (sample.changed) {
                if (!afk_format_timestamp(timestamp, sizeof(timestamp), last_input)) {
                    if (buffer_afk_event(session, timestamp, last_input, 0,
                                         session->afk, pulse_time)) {
                        buffered = false;
                        report_send(session, "persist AFK heartbeat", HTTP_RESULT_ERROR);
                    }
                }
                if (buffered && !afk_format_timestamp(
                        timestamp, sizeof(timestamp), last_input + 0.001)) {
                    if (buffer_afk_event(session, timestamp, last_input + 0.001,
                                         sample.duration, sample.afk, pulse_time)) {
                        buffered = false;
                        report_send(session, "persist AFK heartbeat", HTTP_RESULT_ERROR);
                    }
                }
                if (buffered && checkpoint_buffered_event(session)) {
                    buffered = false;
                    report_send(session, "persist AFK heartbeat", HTTP_RESULT_ERROR);
                }
            } else if (!afk_format_timestamp(timestamp, sizeof(timestamp), last_input)) {
                if (buffer_afk_event(session, timestamp, last_input,
                                     sample.duration, sample.afk, pulse_time)) {
                    buffered = false;
                    report_send(session, "persist AFK heartbeat", HTTP_RESULT_ERROR);
                }
            }
            if (buffered) session->afk = sample.afk;
        }
        session->next_afk = now + session->options.afk_poll_seconds;
    }

    if (now >= session->next_event_checkpoint) {
        if (checkpoint_buffered_event(session))
            report_send(session, "persist AFK heartbeat", HTTP_RESULT_ERROR);
        session->next_event_checkpoint = now + EVENT_CHECKPOINT_SECONDS;
    }

    /* Sample and persist before doing network I/O so a large replay batch
       cannot shift the timestamp of the current AFK observation. */
    if (now >= session->next_event_flush) {
        int result = flush_pending_events(session);
        if (result == HTTP_RESULT_OK)
            result = send_buffered_event(session);
        session->next_event_flush = now + EVENT_RETRY_SECONDS;
        if (result == HTTP_RESULT_UNAUTHORIZED) {
            invalidate_tracking_session(session);
            return result;
        }
    }
    return HTTP_RESULT_OK;
}

double tracker_session_delay(const tracker_session *session) {
    double next = session->next_afk;
    if (session->next_event_flush < next) next = session->next_event_flush;
    if (session->next_event_checkpoint < next) next = session->next_event_checkpoint;
    if (session->window_available && session->next_window < next) next = session->next_window;
    double delay = next - tracker_now_seconds();
    return delay < 0.01 ? 0.01 : delay;
}

double tracker_session_tracked_today(tracker_session *session) {
    time_t current = time(NULL);
    struct tm local;
#ifdef _WIN32
    if (localtime_s(&local, &current) != 0) return -1.0;
#else
    if (localtime_r(&current, &local) == NULL) return -1.0;
#endif
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    time_t start = mktime(&local);
    local.tm_mday += 1;
    local.tm_isdst = -1;
    time_t end = mktime(&local);
    if (start == (time_t)-1 || end == (time_t)-1 || end <= start) return -1.0;

    double day_start = (double)start;
    double day_end = (double)end;
    if (!session->tracked_today_cache_valid ||
        session->tracked_today_start != day_start ||
        session->tracked_today_end != day_end) {
        double seconds = 0.0;
        if (event_store_tracked_seconds(
                &session->events, session->client->base_url,
                session->afk_bucket, day_start, day_end,
                EVENT_MERGE_WINDOW_SECONDS, &seconds))
            return -1.0;
        session->tracked_today_cached = seconds;
        session->tracked_today_start = day_start;
        session->tracked_today_end = day_end;
        session->tracked_today_cache_valid = true;
    }

    double seconds = session->tracked_today_cached;
    if (session->buffered_event_available && session->buffered_event_dirty &&
        strcmp(session->buffered_event.data_json,
               "{\"status\":\"not-afk\"}") == 0) {
        double extra_start = session->buffered_event_checkpoint_end;
        double extra_end = session->buffered_event_end;
        if (extra_start < day_start) extra_start = day_start;
        if (extra_end > day_end) extra_end = day_end;
        if (extra_end > extra_start) seconds += extra_end - extra_start;
    }
    return seconds;
}

int tracker_discard_pending_events(const http_client *client) {
    if (!client || !client->base_url || !client->device_id) return -1;
    char path[2048];
    event_store store;
    if (dirs_event_store_path(path, sizeof(path)) ||
        dirs_create_parent(path) || event_store_open(&store, path))
        return -1;
    int result = event_store_discard_pending(
        &store, client->base_url, client->device_id, (long long)time(NULL));
    event_store_close(&store);
    return result;
}

void tracker_session_cleanup(tracker_session *session) {
    if (session->event_store_available && checkpoint_buffered_event(session))
        report_send(session, "persist AFK heartbeat", HTTP_RESULT_ERROR);
    if (session->window_available) window_cleanup();
    if (session->idle_available) idle_cleanup();
    if (session->event_store_available) event_store_close(&session->events);
    session->window_available = false;
    session->idle_available = false;
    session->event_store_available = false;
}
