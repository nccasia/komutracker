#ifndef TRACKER_AUTH_H
#define TRACKER_AUTH_H

#include "http.h"
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *auth_url;
    const char *client_id;
    const char *redirect_uri;
    int timeout_seconds;
    bool open_browser;
} auth_options;

int auth_get_device_id(char *out, size_t size);
int auth_generate_device_id(char *out, size_t size);
int auth_read_token(char *out, size_t size);
int auth_save_token(const char *token);
int auth_remove_token(void);
int auth_read_profile(char *name, size_t name_size, char *email, size_t email_size);
int auth_save_profile(const char *name, const char *email);
int auth_remove_profile(void);
int auth_build_url(char *out, size_t size, const auth_options *options, const char *device_id);
int auth_login(http_client *client, const auth_options *options,
               char *device_id, size_t device_id_size,
               char *token, size_t token_size, volatile sig_atomic_t *running);
int auth_open_browser(const char *url);

#endif
