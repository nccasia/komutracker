#ifndef AW_HTTP_H
#define AW_HTTP_H

#include <stdbool.h>
#include <signal.h>
#include <stddef.h>

typedef struct {
    const char *base_url;
    const char *token;
    const char *device_id;
    bool verbose;
} http_client;

enum {
    HTTP_AUTH_ERROR = -1,
    HTTP_AUTH_PENDING = 0,
    HTTP_AUTH_OK = 1,
    HTTP_AUTH_UNAUTHORIZED = 2
};

int http_global_init(void);
void http_global_cleanup(void);
void http_set_running_flag(volatile sig_atomic_t *running);
int http_create_bucket(const http_client *client, const char *bucket, const char *client_name,
                       const char *event_type, const char *hostname);
int http_heartbeat(const http_client *client, const char *bucket, const char *timestamp,
                   double duration, bool afk, double pulsetime);
int http_heartbeat_window(const http_client *client, const char *bucket, const char *timestamp,
                          const char *app, const char *title, double pulsetime);
int http_json_escape(const char *source, char *out, size_t size);
int http_auth_poll(const http_client *client, char *token, size_t token_size);
int http_auth_me(const http_client *client, char *name, size_t name_size,
                 char *email, size_t email_size);
int http_auth_delete(const http_client *client);
int http_parse_json_string(const char *json, char *out, size_t size);
int http_parse_profile(const char *json, char *name, size_t name_size,
                       char *email, size_t email_size);

#endif
