#ifndef KOMUTRACKER_EVENT_STORE_H
#define KOMUTRACKER_EVENT_STORE_H

#include <stddef.h>

#define EVENT_STORE_SCOPE_SIZE 2048
#define EVENT_STORE_DEVICE_SIZE 256
#define EVENT_STORE_BUCKET_SIZE 1024
#define EVENT_STORE_TIMESTAMP_SIZE 32
#define EVENT_STORE_KIND_SIZE 32
#define EVENT_STORE_DATA_SIZE 8192

typedef struct {
    void *database;
} event_store;

typedef struct {
    long long id;
    char server_url[EVENT_STORE_SCOPE_SIZE];
    char device_id[EVENT_STORE_DEVICE_SIZE];
    char bucket[EVENT_STORE_BUCKET_SIZE];
    char timestamp[EVENT_STORE_TIMESTAMP_SIZE];
    char kind[EVENT_STORE_KIND_SIZE];
    char data_json[EVENT_STORE_DATA_SIZE];
    double duration;
    double pulsetime;
    long long created_at;
} stored_event;

int event_store_open(event_store *store, const char *path);
void event_store_close(event_store *store);
int event_store_append(event_store *store, const stored_event *event, long long *id);
int event_store_merge(event_store *store, const stored_event *event,
                      double merge_window_seconds, long long *id);
int event_store_next_pending(event_store *store, const char *server_url,
                             const char *device_id, stored_event *event);
int event_store_mark_sent(event_store *store, long long id, long long sent_at);
int event_store_discard_pending(event_store *store, const char *server_url,
                                const char *device_id, long long discarded_at);
int event_store_prune(event_store *store, long long created_before);
int event_store_pending_count(event_store *store, const char *server_url,
                              const char *device_id);
int event_store_tracked_seconds(event_store *store, const char *server_url,
                                const char *bucket, double range_start,
                                double range_end, double merge_window_seconds,
                                double *seconds);

#endif
