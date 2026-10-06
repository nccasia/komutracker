#ifndef KOMUTRACKER_CONFIG_H
#define KOMUTRACKER_CONFIG_H

typedef struct {
    const char *server_url;
    const char *auth_url;
    const char *oauth_client_id;
    const char *oauth_redirect_uri;
    int auth_timeout_seconds;
    double afk_timeout_seconds;
    double afk_poll_seconds;
    double window_poll_seconds;
} komutracker_config;

extern const komutracker_config KOMUTRACKER_CONFIG;
extern const komutracker_config KOMUTRACKER_TEST_CONFIG;

#endif
