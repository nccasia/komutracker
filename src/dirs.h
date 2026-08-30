#ifndef TRACKER_DIRS_H
#define TRACKER_DIRS_H

#include <stddef.h>

int dirs_device_path(char *out, size_t size);
int dirs_token_path(char *out, size_t size);
int dirs_create_parent(const char *path);

#endif
