#include "event_store.h"

#include <sqlite3.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif

static sqlite3 *database(event_store *store) {
    return (sqlite3 *)store->database;
}

static int execute(sqlite3 *db, const char *sql) {
    char *message = NULL;
    int result = sqlite3_exec(db, sql, NULL, NULL, &message);
    if (message) sqlite3_free(message);
    return result == SQLITE_OK ? 0 : -1;
}

int event_store_open(event_store *store, const char *path) {
    if (!store || !path) return -1;
    memset(store, 0, sizeof(*store));

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)
        != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return -1;
    }
#ifndef _WIN32
    if (strcmp(path, ":memory:") && chmod(path, 0600) != 0) {
        sqlite3_close(db);
        return -1;
    }
#endif
    sqlite3_busy_timeout(db, 2000);
    if (execute(db, "PRAGMA journal_mode=WAL") ||
        execute(db, "PRAGMA synchronous=NORMAL") ||
        execute(db,
            "CREATE TABLE IF NOT EXISTS heartbeat_events ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "server_url TEXT NOT NULL,"
            "device_id TEXT NOT NULL,"
            "bucket TEXT NOT NULL,"
            "timestamp TEXT NOT NULL,"
            "kind TEXT NOT NULL,"
            "duration REAL NOT NULL,"
            "data_json TEXT NOT NULL,"
            "pulsetime REAL NOT NULL,"
            "created_at INTEGER NOT NULL,"
            "sent_at INTEGER,"
            "discarded_at INTEGER)") ||
        execute(db,
            "CREATE INDEX IF NOT EXISTS heartbeat_events_pending "
            "ON heartbeat_events(server_url, device_id, sent_at, discarded_at, id)") ||
        execute(db,
            "CREATE INDEX IF NOT EXISTS heartbeat_events_created "
            "ON heartbeat_events(created_at)") ||
        execute(db,
            "CREATE INDEX IF NOT EXISTS heartbeat_events_timeline_v2 "
            "ON heartbeat_events(server_url, bucket, kind, id)")) {
        sqlite3_close(db);
        return -1;
    }

    store->database = db;
    return 0;
}

void event_store_close(event_store *store) {
    if (!store || !store->database) return;
    sqlite3_close(database(store));
    store->database = NULL;
}

int event_store_append(event_store *store, const stored_event *event, long long *id) {
    static const char sql[] =
        "INSERT INTO heartbeat_events "
        "(server_url,device_id,bucket,timestamp,kind,duration,data_json,pulsetime,created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?)";
    if (!store || !store->database || !event) return -1;

    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database(store), sql, -1, &statement, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(statement, 1, event->server_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, event->device_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, event->bucket, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, event->timestamp, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, event->kind, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 6, event->duration);
    sqlite3_bind_text(statement, 7, event->data_json, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 8, event->pulsetime);
    sqlite3_bind_int64(statement, 9, event->created_at);
    int result = sqlite3_step(statement) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(statement);
    if (!result && id) *id = sqlite3_last_insert_rowid(database(store));
    return result;
}

static int timestamp_days(sqlite3 *db, const char *timestamp, double *days) {
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(db, "SELECT julianday(?)", -1, &statement, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(statement, 1, timestamp, -1, SQLITE_TRANSIENT);
    int result = -1;
    if (sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) != SQLITE_NULL) {
        *days = sqlite3_column_double(statement, 0);
        result = isfinite(*days) ? 0 : -1;
    }
    sqlite3_finalize(statement);
    return result;
}

static int update_merged(sqlite3 *db, long long id, const char *timestamp,
                         double duration, double pulsetime, long long created_at) {
    static const char sql[] =
        "UPDATE heartbeat_events SET timestamp=?,duration=?,pulsetime=?,"
        "created_at=?,sent_at=NULL WHERE id=?";
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(statement, 1, timestamp, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 2, duration);
    sqlite3_bind_double(statement, 3, pulsetime);
    sqlite3_bind_int64(statement, 4, created_at);
    sqlite3_bind_int64(statement, 5, id);
    int result = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1
        ? 0 : -1;
    sqlite3_finalize(statement);
    return result;
}

int event_store_merge(event_store *store, const stored_event *event,
                      double merge_window_seconds, long long *id) {
    static const char latest_sql[] =
        "SELECT id,timestamp,kind,data_json,duration,pulsetime,created_at,"
        "julianday(timestamp) AS start_day,"
        "julianday(timestamp)+duration/86400.0 AS end_day "
        "FROM heartbeat_events WHERE server_url=? AND device_id=? AND bucket=? "
        "AND discarded_at IS NULL "
        "ORDER BY end_day DESC,start_day DESC,id DESC LIMIT 1";
    if (!store || !store->database || !event || !isfinite(event->duration) ||
        event->duration < 0 || !isfinite(merge_window_seconds) ||
        merge_window_seconds < 0) return -1;

    sqlite3 *db = database(store);
    double event_start = 0;
    if (timestamp_days(db, event->timestamp, &event_start)) return -1;
    double event_end = event_start + event->duration / 86400.0;
    if (execute(db, "BEGIN IMMEDIATE")) return -1;

    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(db, latest_sql, -1, &statement, NULL) != SQLITE_OK) {
        execute(db, "ROLLBACK");
        return -1;
    }
    sqlite3_bind_text(statement, 1, event->server_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, event->device_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, event->bucket, -1, SQLITE_TRANSIENT);

    int step = sqlite3_step(statement);
    int result = -1;
    long long merged_id = 0;
    if (step == SQLITE_DONE) {
        sqlite3_finalize(statement);
        result = event_store_append(store, event, &merged_id);
    } else if (step == SQLITE_ROW) {
        merged_id = sqlite3_column_int64(statement, 0);
        const char *previous_timestamp = (const char *)sqlite3_column_text(statement, 1);
        const char *previous_kind = (const char *)sqlite3_column_text(statement, 2);
        const char *previous_data = (const char *)sqlite3_column_text(statement, 3);
        double previous_pulsetime = sqlite3_column_double(statement, 5);
        long long previous_created_at = sqlite3_column_int64(statement, 6);
        double previous_start = sqlite3_column_double(statement, 7);
        double previous_end = sqlite3_column_double(statement, 8);
        int compatible = previous_timestamp && previous_kind && previous_data &&
            !strcmp(previous_kind, event->kind) &&
            !strcmp(previous_data, event->data_json);
        double gap_days = event_start > previous_end
            ? event_start - previous_end
            : (previous_start > event_end ? previous_start - event_end : 0.0);

        if (compatible && gap_days * 86400.0 <= merge_window_seconds) {
            char merged_timestamp[EVENT_STORE_TIMESTAMP_SIZE];
            const char *earliest = event_start < previous_start
                ? event->timestamp : previous_timestamp;
            int timestamp_length = snprintf(
                merged_timestamp, sizeof(merged_timestamp), "%s", earliest);
            int timestamp_ok = timestamp_length >= 0 &&
                timestamp_length < (int)sizeof(merged_timestamp);
            double merged_start = event_start < previous_start ? event_start : previous_start;
            double merged_end = event_end > previous_end ? event_end : previous_end;
            double merged_pulsetime = event->pulsetime > previous_pulsetime
                ? event->pulsetime : previous_pulsetime;
            long long merged_created_at = event->created_at > previous_created_at
                ? event->created_at : previous_created_at;
            sqlite3_finalize(statement);
            result = timestamp_ok ? update_merged(
                db, merged_id, merged_timestamp,
                (merged_end - merged_start) * 86400.0,
                merged_pulsetime, merged_created_at) : -1;
        } else {
            sqlite3_finalize(statement);
            result = event_store_append(store, event, &merged_id);
        }
    } else {
        sqlite3_finalize(statement);
    }

    if (result == 0 && execute(db, "COMMIT") == 0) {
        if (id) *id = merged_id;
        return 0;
    }
    execute(db, "ROLLBACK");
    return -1;
}

static int copy_text(sqlite3_stmt *statement, int column, char *out, size_t size) {
    const unsigned char *value = sqlite3_column_text(statement, column);
    if (!value || strlen((const char *)value) >= size) return -1;
    strcpy(out, (const char *)value);
    return 0;
}

int event_store_next_pending(event_store *store, const char *server_url,
                             const char *device_id, stored_event *event) {
    static const char sql[] =
        "SELECT id,server_url,device_id,bucket,timestamp,kind,duration,data_json,pulsetime,created_at "
        "FROM heartbeat_events WHERE server_url=? AND device_id=? "
        "AND sent_at IS NULL AND discarded_at IS NULL "
        "ORDER BY id LIMIT 1";
    if (!store || !store->database || !server_url || !device_id || !event) return -1;

    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database(store), sql, -1, &statement, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(statement, 1, server_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, device_id, -1, SQLITE_TRANSIENT);
    int step = sqlite3_step(statement);
    if (step == SQLITE_DONE) {
        sqlite3_finalize(statement);
        return 0;
    }
    if (step != SQLITE_ROW) {
        sqlite3_finalize(statement);
        return -1;
    }

    memset(event, 0, sizeof(*event));
    event->id = sqlite3_column_int64(statement, 0);
    int invalid = copy_text(statement, 1, event->server_url, sizeof(event->server_url)) ||
        copy_text(statement, 2, event->device_id, sizeof(event->device_id)) ||
        copy_text(statement, 3, event->bucket, sizeof(event->bucket)) ||
        copy_text(statement, 4, event->timestamp, sizeof(event->timestamp)) ||
        copy_text(statement, 5, event->kind, sizeof(event->kind)) ||
        copy_text(statement, 7, event->data_json, sizeof(event->data_json));
    event->duration = sqlite3_column_double(statement, 6);
    event->pulsetime = sqlite3_column_double(statement, 8);
    event->created_at = sqlite3_column_int64(statement, 9);
    sqlite3_finalize(statement);
    return invalid ? -1 : 1;
}

int event_store_mark_sent(event_store *store, long long id, long long sent_at) {
    static const char sql[] = "UPDATE heartbeat_events SET sent_at=? WHERE id=?";
    if (!store || !store->database) return -1;
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database(store), sql, -1, &statement, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(statement, 1, sent_at);
    sqlite3_bind_int64(statement, 2, id);
    int result = sqlite3_step(statement) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(statement);
    return result;
}

int event_store_discard_pending(event_store *store, const char *server_url,
                                const char *device_id, long long discarded_at) {
    static const char sql[] =
        "UPDATE heartbeat_events SET discarded_at=? "
        "WHERE server_url=? AND device_id=? "
        "AND sent_at IS NULL AND discarded_at IS NULL";
    if (!store || !store->database || !server_url || !device_id) return -1;
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database(store), sql, -1, &statement, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(statement, 1, discarded_at);
    sqlite3_bind_text(statement, 2, server_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, device_id, -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(statement) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(statement);
    return result;
}

int event_store_prune(event_store *store, long long created_before) {
    static const char sql[] = "DELETE FROM heartbeat_events WHERE created_at < ?";
    if (!store || !store->database) return -1;
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database(store), sql, -1, &statement, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(statement, 1, created_before);
    int result = sqlite3_step(statement) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(statement);
    return result;
}

int event_store_pending_count(event_store *store, const char *server_url,
                              const char *device_id) {
    static const char sql[] =
        "SELECT COUNT(*) FROM heartbeat_events "
        "WHERE server_url=? AND device_id=? "
        "AND sent_at IS NULL AND discarded_at IS NULL";
    if (!store || !store->database || !server_url || !device_id) return -1;
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database(store), sql, -1, &statement, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(statement, 1, server_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, device_id, -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(statement) == SQLITE_ROW
        ? sqlite3_column_int(statement, 0) : -1;
    sqlite3_finalize(statement);
    return result;
}

static double interval_gap(double first_start, double first_end,
                           double second_start, double second_end) {
    if (second_start > first_end) return second_start - first_end;
    if (first_start > second_end) return first_start - second_end;
    return 0.0;
}

static double tracked_overlap(const char *data_json, double start, double end,
                              double range_start, double range_end) {
    if (strcmp(data_json, "{\"status\":\"not-afk\"}")) return 0.0;
    double overlap_start = start > range_start ? start : range_start;
    double overlap_end = end < range_end ? end : range_end;
    return overlap_end > overlap_start ? overlap_end - overlap_start : 0.0;
}

int event_store_tracked_seconds(event_store *store, const char *server_url,
                                const char *bucket, double range_start,
                                double range_end, double merge_window_seconds,
                                double *seconds) {
    static const char sql[] =
        "SELECT data_json,(julianday(timestamp)-2440587.5)*86400.0,duration "
        "FROM heartbeat_events WHERE server_url=? AND bucket=? "
        "AND kind='afkstatus' AND discarded_at IS NULL ORDER BY id";
    if (!store || !store->database || !server_url || !bucket || !seconds ||
        !isfinite(range_start) || !isfinite(range_end) || range_end <= range_start ||
        !isfinite(merge_window_seconds) || merge_window_seconds < 0) return -1;

    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(database(store), sql, -1, &statement, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(statement, 1, server_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, bucket, -1, SQLITE_TRANSIENT);

    double total = 0.0, segment_start = 0.0, segment_end = 0.0;
    char segment_data[EVENT_STORE_DATA_SIZE] = {0};
    int has_segment = 0;
    int step;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        const char *data = (const char *)sqlite3_column_text(statement, 0);
        if (!data || sqlite3_column_type(statement, 1) == SQLITE_NULL) {
            sqlite3_finalize(statement);
            return -1;
        }
        double start = sqlite3_column_double(statement, 1);
        double duration = sqlite3_column_double(statement, 2);
        double end = start + duration;
        if (!isfinite(start) || !isfinite(duration) || duration < 0 ||
            strlen(data) >= sizeof(segment_data)) {
            sqlite3_finalize(statement);
            return -1;
        }

        int merge = has_segment && !strcmp(segment_data, data) &&
            interval_gap(segment_start, segment_end, start, end)
                <= merge_window_seconds + 0.001;
        if (!merge) {
            if (has_segment)
                total += tracked_overlap(segment_data, segment_start, segment_end,
                                         range_start, range_end);
            strcpy(segment_data, data);
            segment_start = start;
            segment_end = end;
            has_segment = 1;
        } else {
            if (start < segment_start) segment_start = start;
            if (end > segment_end) segment_end = end;
        }
    }
    if (step != SQLITE_DONE) {
        sqlite3_finalize(statement);
        return -1;
    }
    if (has_segment)
        total += tracked_overlap(segment_data, segment_start, segment_end,
                                 range_start, range_end);
    sqlite3_finalize(statement);
    *seconds = total;
    return 0;
}
