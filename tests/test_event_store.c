#include "event_store.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static stored_event sample(const char *device, const char *timestamp, long long created_at) {
    stored_event event = {0};
    strcpy(event.server_url, "https://tracker.example");
    strcpy(event.device_id, device);
    strcpy(event.bucket, "aw-watcher-afk_test");
    strcpy(event.timestamp, timestamp);
    strcpy(event.kind, "afkstatus");
    strcpy(event.data_json, "{\"status\":\"not-afk\"}");
    event.duration = 10.0;
    event.pulsetime = 15.0;
    event.created_at = created_at;
    return event;
}

static void remove_database(const char *path) {
    char sidecar[256];
    remove(path);
    snprintf(sidecar, sizeof(sidecar), "%s-wal", path);
    remove(sidecar);
    snprintf(sidecar, sizeof(sidecar), "%s-shm", path);
    remove(sidecar);
}

int main(void) {
    event_store store;
    assert(event_store_open(&store, ":memory:") == 0);

    stored_event first = sample("device-a", "2026-10-10T01:00:00.000Z", 100);
    stored_event second = sample("device-a", "2026-10-10T01:00:10.000Z", 200);
    stored_event other = sample("device-b", "2026-10-10T01:00:20.000Z", 300);
    long long first_id = 0;
    assert(event_store_append(&store, &first, &first_id) == 0);
    assert(first_id > 0);
    assert(event_store_append(&store, &second, NULL) == 0);
    assert(event_store_append(&store, &other, NULL) == 0);
    assert(event_store_pending_count(&store, first.server_url, "device-a") == 2);

    stored_event pending;
    assert(event_store_next_pending(&store, first.server_url, "device-a", &pending) == 1);
    assert(pending.id == first_id);
    assert(strcmp(pending.timestamp, first.timestamp) == 0);
    assert(strcmp(pending.data_json, first.data_json) == 0);
    assert(event_store_mark_sent(&store, pending.id, 400) == 0);
    assert(event_store_pending_count(&store, first.server_url, "device-a") == 1);
    assert(event_store_next_pending(&store, first.server_url, "device-a", &pending) == 1);
    assert(strcmp(pending.timestamp, second.timestamp) == 0);
    assert(event_store_discard_pending(
        &store, first.server_url, "device-a", 450) == 0);
    assert(event_store_pending_count(&store, first.server_url, "device-a") == 0);
    assert(event_store_next_pending(
        &store, first.server_url, "device-a", &pending) == 0);
    assert(event_store_pending_count(&store, first.server_url, "device-b") == 1);

    assert(event_store_prune(&store, 250) == 0);
    assert(event_store_pending_count(&store, first.server_url, "device-a") == 0);
    assert(event_store_pending_count(&store, first.server_url, "device-b") == 1);
    event_store_close(&store);

    assert(event_store_open(&store, ":memory:") == 0);
    stored_event merged_first = sample(
        "merge-device", "2026-10-10T01:00:00.000Z", 100);
    stored_event merged_second = sample(
        "merge-device", "2026-10-10T01:00:20.000Z", 200);
    long long merged_id = 0, second_id = 0;
    assert(event_store_merge(&store, &merged_first, 15.0, &merged_id) == 0);
    assert(event_store_mark_sent(&store, merged_id, 150) == 0);
    assert(event_store_merge(&store, &merged_second, 15.0, &second_id) == 0);
    assert(second_id == merged_id);
    assert(event_store_pending_count(
        &store, merged_first.server_url, merged_first.device_id) == 1);
    assert(event_store_next_pending(
        &store, merged_first.server_url, merged_first.device_id, &pending) == 1);
    assert(strcmp(pending.timestamp, merged_first.timestamp) == 0);
    assert(pending.duration > 29.999 && pending.duration < 30.001);

    assert(event_store_mark_sent(&store, merged_id, 250) == 0);
    stored_event transition = sample(
        "merge-device", "2026-10-10T01:00:31.000Z", 300);
    strcpy(transition.data_json, "{\"status\":\"afk\"}");
    long long transition_id = 0;
    assert(event_store_merge(&store, &transition, 15.0, &transition_id) == 0);
    assert(transition_id != merged_id);

    stored_event far_first = sample(
        "far-device", "2026-10-10T02:00:00.000Z", 400);
    stored_event far_second = sample(
        "far-device", "2026-10-10T02:01:00.000Z", 500);
    assert(event_store_merge(&store, &far_first, 15.0, NULL) == 0);
    assert(event_store_merge(&store, &far_second, 15.0, NULL) == 0);
    assert(event_store_pending_count(
        &store, far_first.server_url, far_first.device_id) == 2);
    event_store_close(&store);

    /* Reconstruct the timeline from legacy, unmerged heartbeat rows and clip
       the resulting not-afk intervals to the requested local-day range. */
    assert(event_store_open(&store, ":memory:") == 0);
    stored_event active_one = sample(
        "summary-device", "2026-10-10T01:00:00.000Z", 100);
    active_one.duration = 0;
    stored_event active_two = sample(
        "summary-device", "2026-10-10T01:01:00.000Z", 200);
    active_two.duration = 0;
    stored_event idle = sample(
        "summary-device", "2026-10-10T01:02:00.000Z", 300);
    strcpy(idle.data_json, "{\"status\":\"afk\"}");
    idle.duration = 60;
    stored_event active_three = sample(
        "summary-device", "2026-10-10T01:03:00.000Z", 400);
    active_three.duration = 120;
    assert(event_store_append(&store, &active_one, NULL) == 0);
    assert(event_store_append(&store, &active_two, NULL) == 0);
    assert(event_store_append(&store, &idle, NULL) == 0);
    assert(event_store_append(&store, &active_three, NULL) == 0);
    double tracked = 0;
    assert(event_store_tracked_seconds(
        &store, active_one.server_url, active_one.bucket,
        1791594030.0, 1791594210.0, 370.0, &tracked) == 0);
    assert(tracked > 59.999 && tracked < 60.001);
    event_store_close(&store);

    /* Discarded events stay in the journal for audit, but are neither replayed
       nor counted as credited local time, and a new event cannot revive them. */
    assert(event_store_open(&store, ":memory:") == 0);
    stored_event discarded = sample(
        "discard-device", "2026-10-10T03:00:00.000Z", 500);
    assert(event_store_append(&store, &discarded, NULL) == 0);
    assert(event_store_discard_pending(
        &store, discarded.server_url, discarded.device_id, 600) == 0);
    assert(event_store_pending_count(
        &store, discarded.server_url, discarded.device_id) == 0);
    assert(event_store_tracked_seconds(
        &store, discarded.server_url, discarded.bucket,
        0.0, 4102444800.0, 370.0, &tracked) == 0);
    assert(tracked == 0.0);
    stored_event replacement = sample(
        "discard-device", "2026-10-10T03:00:20.000Z", 700);
    assert(event_store_merge(&store, &replacement, 370.0, NULL) == 0);
    assert(event_store_pending_count(
        &store, replacement.server_url, replacement.device_id) == 1);
    event_store_close(&store);

    const char *path = "event-store-persistence-test.db";
    remove_database(path);
    assert(event_store_open(&store, path) == 0);
    assert(event_store_append(&store, &first, NULL) == 0);
    event_store_close(&store);
    assert(event_store_open(&store, path) == 0);
    assert(event_store_pending_count(&store, first.server_url, "device-a") == 1);
    event_store_close(&store);
    remove_database(path);

    /* Existing journals are migrated in place without replaying discarded rows. */
    const char *legacy_path = "event-store-legacy-test.db";
    remove_database(legacy_path);
    sqlite3 *legacy = NULL;
    assert(sqlite3_open(legacy_path, &legacy) == SQLITE_OK);
    assert(sqlite3_exec(legacy,
        "CREATE TABLE heartbeat_events ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,server_url TEXT NOT NULL,"
        "device_id TEXT NOT NULL,bucket TEXT NOT NULL,timestamp TEXT NOT NULL,"
        "kind TEXT NOT NULL,duration REAL NOT NULL,data_json TEXT NOT NULL,"
        "pulsetime REAL NOT NULL,created_at INTEGER NOT NULL,sent_at INTEGER)",
        NULL, NULL, NULL) == SQLITE_OK);
    sqlite3_close(legacy);
    assert(event_store_open(&store, legacy_path) == 0);
    assert(event_store_append(&store, &first, NULL) == 0);
    assert(event_store_discard_pending(
        &store, first.server_url, first.device_id, 800) == 0);
    assert(event_store_pending_count(
        &store, first.server_url, first.device_id) == 0);
    event_store_close(&store);
    remove_database(legacy_path);
    return 0;
}
