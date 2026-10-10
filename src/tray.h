#ifndef KOMUTRACKER_TRAY_H
#define KOMUTRACKER_TRAY_H

#include <stdbool.h>

typedef enum {
    TRAY_LOGGED_OUT,
    TRAY_AUTHENTICATING,
    TRAY_CONNECTING,
    TRAY_TRACKING,
    TRAY_CONNECTION_ERROR
} tray_status;

typedef struct {
    void (*auth_action)(void *context);
    void (*logout)(void *context);
    void (*open_dashboard)(void *context);
    void (*quit)(void *context);
    void *context;
} tray_callbacks;

typedef struct {
    tray_status status;
    bool logged_in;
    char account_name[512];
    char status_text[256];
    char tracked_text[128];
} tray_view;

int tray_init(const tray_callbacks *callbacks);
void tray_update(const tray_view *view);
void tray_poll(void);
void tray_cleanup(void);

#endif
