#ifndef TRACKER_WINDOW_H
#define TRACKER_WINDOW_H

#define WINDOW_APP_SIZE 512
#define WINDOW_TITLE_SIZE 2048

typedef struct {
    char app[WINDOW_APP_SIZE];
    char title[WINDOW_TITLE_SIZE];
} window_info;

int window_init(void);
int window_get_current(window_info *out);
void window_cleanup(void);

#endif
