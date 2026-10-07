#include "auth.h"
#include <assert.h>
#include <string.h>

int main(void) {
    char value[128];
    assert(http_parse_json_string("null", value, sizeof(value)) == HTTP_AUTH_PENDING);
    assert(http_parse_json_string("\"token-value\"", value, sizeof(value)) == HTTP_AUTH_OK);
    assert(strcmp(value, "token-value") == 0);

    char name[128], email[128];
    assert(http_parse_profile("{\"name\":\"Test User\",\"email\":\"test@example.com\"}",
                              name, sizeof(name), email, sizeof(email)) == 0);
    assert(strcmp(name, "Test User") == 0);
    assert(strcmp(email, "test@example.com") == 0);

    char first_device[65], second_device[65];
    assert(auth_generate_device_id(first_device, sizeof(first_device)) == 0);
    assert(auth_generate_device_id(second_device, sizeof(second_device)) == 0);
    assert(strlen(first_device) == 64);
    assert(strlen(second_device) == 64);
    assert(strcmp(first_device, second_device) != 0);

    /* Test parsing activity events for today active time */
    double active_sec = -1.0;
    assert(http_parse_events_active_seconds("[]", 0, 2000000000L, &active_sec) == 0);
    assert(active_sec == 0.0);

    const char *events_json =
        "[{\"id\":\"1\",\"status\":\"not-afk\",\"startAt\":\"2026-10-07T01:00:00.000Z\",\"endAt\":\"2026-10-07T02:30:00.000Z\"},"
        "{\"id\":\"2\",\"status\":\"afk\",\"startAt\":\"2026-10-07T02:30:00.000Z\",\"endAt\":\"2026-10-07T02:45:00.000Z\"},"
        "{\"id\":\"3\",\"status\":\"not-afk\",\"startAt\":\"2026-10-07T02:45:00.000Z\",\"endAt\":\"2026-10-07T04:15:30.000Z\"}]";

    /* Range covering the whole day */
    active_sec = 0.0;
    assert(http_parse_events_active_seconds(events_json, 1791331200L, 1791417600L, &active_sec) == 0);
    /* 5400s (01:00 to 02:30) + 5430s (02:45 to 04:15:30) = 10830s */
    assert(active_sec == 10830.0);

    /* Range partially clipping events (02:00:00 to 03:00:00) */
    active_sec = 0.0;
    assert(http_parse_events_active_seconds(events_json, 1791338400L, 1791342000L, &active_sec) == 0);
    /* 1800s (02:00 to 02:30) + 900s (02:45 to 03:00) = 2700s */
    assert(active_sec == 2700.0);

    return 0;
}
