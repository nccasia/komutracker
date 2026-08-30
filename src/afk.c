#include "afk.h"

#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#define timegm _mkgmtime
#endif

afk_sample afk_update(bool was_afk, double idle, double timeout) {
    afk_sample sample = {0};
    sample.afk = idle >= timeout;
    sample.changed = sample.afk != was_afk;
    sample.idle_seconds = idle;
    sample.event_offset = -idle;
    sample.duration = sample.afk ? idle : 0.0;
    return sample;
}

int afk_format_timestamp(char *buffer, size_t size, double unix_seconds) {
    time_t whole = (time_t)unix_seconds;
    int millis = (int)((unix_seconds - (double)whole) * 1000.0 + 0.5);
    if (millis >= 1000) {
        whole++;
        millis = 0;
    }
    struct tm utc;
#ifdef _WIN32
    if (gmtime_s(&utc, &whole) != 0) return -1;
#else
    if (gmtime_r(&whole, &utc) == NULL) return -1;
#endif
    return snprintf(buffer, size, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                    utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                    utc.tm_hour, utc.tm_min, utc.tm_sec, millis) >= (int)size ? -1 : 0;
}
