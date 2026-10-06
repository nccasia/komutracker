#ifndef KOMUTRACKER_TRACKER_H
#define KOMUTRACKER_TRACKER_H

#include "http.h"

#include <stdbool.h>
#include <stddef.h>

typedef void (*tracker_send_callback)(void *context, const char *event, int result);

typedef struct {
    double afk_timeout_seconds;
    double afk_poll_seconds;
    double window_poll_seconds;
    bool exclude_window_title;
    tracker_send_callback on_send;
    void *callback_context;
} tracker_options;

typedef struct {
    http_client *client;
    tracker_options options;
    char hostname[256];
    char username[512];
    char afk_bucket[1024];
    char window_bucket[1024];
    bool idle_available;
    bool window_available;
    bool afk;
    double next_afk;
    double next_window;
} tracker_session;

double tracker_now_seconds(void);
void tracker_sleep_seconds(double seconds);
int tracker_session_init(tracker_session *session, http_client *client,
                         const char *email, const tracker_options *options);
int tracker_session_poll(tracker_session *session);
double tracker_session_delay(const tracker_session *session);
void tracker_session_cleanup(tracker_session *session);

#endif
