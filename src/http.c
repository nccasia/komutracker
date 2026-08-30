#include "http.h"

#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *data; size_t size; size_t capacity; } response_buffer;
static volatile sig_atomic_t *running_flag;

static int transfer_progress(void *context, curl_off_t download_total, curl_off_t download_now,
                             curl_off_t upload_total, curl_off_t upload_now) {
    (void)context; (void)download_total; (void)download_now; (void)upload_total; (void)upload_now;
    return running_flag && !*running_flag ? 1 : 0;
}

static size_t capture(void *data, size_t size, size_t count, void *context) {
    response_buffer *buffer = context;
    size_t bytes = size * count;
    if (buffer->size + bytes + 1 > buffer->capacity) return 0;
    memcpy(buffer->data + buffer->size, data, bytes);
    buffer->size += bytes;
    buffer->data[buffer->size] = '\0';
    return bytes;
}

static int request(const http_client *client, const char *url, const char *method,
                   const char *body, char *response, size_t response_size, long *status) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[4096], device[512];
    if (client->token && *client->token) {
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", client->token);
        headers = curl_slist_append(headers, auth);
    }
    if (client->device_id && *client->device_id) {
        snprintf(device, sizeof(device), "Device-Id: %s", client->device_id);
        headers = curl_slist_append(headers, device);
    }
    char sink[1];
    response_buffer buffer = { response ? response : sink, 0, response ? response_size : sizeof(sink) };
    buffer.data[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    if (body) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer_progress);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "komutracker/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, capture);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    CURLcode result = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (status) *status = code;
    if (result != CURLE_OK && client->verbose)
        fprintf(stderr, "HTTP request failed: %s\n", curl_easy_strerror(result));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result == CURLE_OK ? 0 : -1;
}

int http_global_init(void) { return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : -1; }
void http_global_cleanup(void) { curl_global_cleanup(); }
void http_set_running_flag(volatile sig_atomic_t *running) { running_flag = running; }

static int post_ok(const http_client *client, const char *url, const char *body) {
    long status;
    char response[1024];
    return request(client, url, "POST", body, response, sizeof(response), &status) == 0 &&
           status >= 200 && status < 300 ? 0 : -1;
}

int http_create_bucket(const http_client *client, const char *bucket, const char *client_name,
                       const char *event_type, const char *hostname) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    char *escaped = curl_easy_escape(curl, bucket, 0);
    char url[2048], body[2048];
    if (!escaped) { curl_easy_cleanup(curl); return -1; }
    snprintf(url, sizeof(url), "%s/api/0/buckets/%s", client->base_url, escaped);
    snprintf(body, sizeof(body), "{\"type\":\"%s\",\"client\":\"%s\",\"hostname\":\"%s\"}",
             event_type, client_name, hostname);
    curl_free(escaped); curl_easy_cleanup(curl);
    long status;
    char response[1024];
    int result = request(client, url, "POST", body, response, sizeof(response), &status);
    if (result || ((status < 200 || status >= 300) && status != 409)) {
        if (client->verbose)
            fprintf(stderr, "Bucket creation failed: HTTP %ld%s%s\n", status,
                    response[0] ? ": " : "", response);
        return -1;
    }
    return 0;
}

int http_json_escape(const char *source, char *out, size_t size) {
    static const char hex[] = "0123456789abcdef";
    size_t used = 0;
    for (const unsigned char *p = (const unsigned char *)source; *p; p++) {
        const char *escape = NULL;
        char unicode[7];
        if (*p == '"') escape = "\\\"";
        else if (*p == '\\') escape = "\\\\";
        else if (*p == '\n') escape = "\\n";
        else if (*p == '\r') escape = "\\r";
        else if (*p == '\t') escape = "\\t";
        else if (*p < 0x20) {
            unicode[0] = '\\'; unicode[1] = 'u'; unicode[2] = '0'; unicode[3] = '0';
            unicode[4] = hex[*p >> 4]; unicode[5] = hex[*p & 15]; unicode[6] = '\0';
            escape = unicode;
        }
        if (escape) {
            size_t length = strlen(escape);
            if (used + length >= size) return -1;
            memcpy(out + used, escape, length); used += length;
        } else {
            if (used + 1 >= size) return -1;
            out[used++] = (char)*p;
        }
    }
    out[used] = '\0';
    return 0;
}

int http_heartbeat_window(const http_client *client, const char *bucket, const char *timestamp,
                          const char *app, const char *title, double pulsetime) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    char *escaped_bucket = curl_easy_escape(curl, bucket, 0);
    char escaped_app[2048], escaped_title[8192], url[2048], body[12288];
    if (!escaped_bucket || http_json_escape(app, escaped_app, sizeof(escaped_app)) ||
        http_json_escape(title, escaped_title, sizeof(escaped_title))) {
        curl_free(escaped_bucket); curl_easy_cleanup(curl); return -1;
    }
    snprintf(url, sizeof(url), "%s/api/0/buckets/%s/heartbeat?pulsetime=%.3f",
             client->base_url, escaped_bucket, pulsetime);
    snprintf(body, sizeof(body),
             "{\"timestamp\":\"%s\",\"duration\":0,\"data\":{\"app\":\"%s\",\"title\":\"%s\"}}",
             timestamp, escaped_app, escaped_title);
    curl_free(escaped_bucket); curl_easy_cleanup(curl);
    return post_ok(client, url, body);
}

int http_heartbeat(const http_client *client, const char *bucket, const char *timestamp,
                   double duration, bool afk, double pulsetime) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    char *escaped = curl_easy_escape(curl, bucket, 0);
    char url[2048], body[2048];
    if (!escaped) { curl_easy_cleanup(curl); return -1; }
    snprintf(url, sizeof(url), "%s/api/0/buckets/%s/heartbeat?pulsetime=%.3f", client->base_url, escaped, pulsetime);
    snprintf(body, sizeof(body), "{\"timestamp\":\"%s\",\"duration\":%.3f,\"data\":{\"status\":\"%s\"}}", timestamp, duration, afk ? "afk" : "not-afk");
    curl_free(escaped); curl_easy_cleanup(curl);
    return post_ok(client, url, body);
}

int http_parse_json_string(const char *json, char *out, size_t size) {
    while (isspace((unsigned char)*json)) json++;
    if (!strncmp(json, "null", 4)) return HTTP_AUTH_PENDING;
    if (*json++ != '"') return HTTP_AUTH_ERROR;
    size_t used = 0;
    while (*json && *json != '"') {
        char value = *json++;
        if (value == '\\') {
            value = *json++;
            if (value == 'n') value = '\n'; else if (value == 'r') value = '\r'; else if (value == 't') value = '\t';
        }
        if (used + 1 >= size) return HTTP_AUTH_ERROR;
        out[used++] = value;
    }
    if (*json != '"' || !used) return HTTP_AUTH_ERROR;
    out[used] = '\0';
    return HTTP_AUTH_OK;
}

static int field(const char *json, const char *key, char *out, size_t size) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *start = strstr(json, pattern);
    if (!start) return -1;
    start = strchr(start + strlen(pattern), ':');
    if (!start) return -1;
    return http_parse_json_string(start + 1, out, size) == HTTP_AUTH_OK ? 0 : -1;
}

int http_parse_profile(const char *json, char *name, size_t name_size, char *email, size_t email_size) {
    char discard[2];
    if (!name) { name = discard; name_size = sizeof(discard); }
    if (!email) { email = discard; email_size = sizeof(discard); }
    return field(json, "name", name, name_size) || field(json, "email", email, email_size) ? -1 : 0;
}

int http_auth_poll(const http_client *client, char *token, size_t token_size) {
    char url[2048], body[1024], response[8192]; long status;
    snprintf(url, sizeof(url), "%s/api/0/auth", client->base_url);
    snprintf(body, sizeof(body), "{\"device_id\":\"%s\"}", client->device_id);
    if (request(client, url, "POST", body, response, sizeof(response), &status)) return HTTP_AUTH_ERROR;
    if (status == 401 || status == 403) return HTTP_AUTH_UNAUTHORIZED;
    if (status < 200 || status >= 300) return HTTP_AUTH_ERROR;
    return http_parse_json_string(response, token, token_size);
}

int http_auth_me(const http_client *client, char *name, size_t name_size, char *email, size_t email_size) {
    char url[2048], response[8192]; long status;
    snprintf(url, sizeof(url), "%s/api/0/auth/me", client->base_url);
    if (request(client, url, "GET", NULL, response, sizeof(response), &status)) return HTTP_AUTH_ERROR;
    if (status == 401 || status == 403) return HTTP_AUTH_UNAUTHORIZED;
    if (status < 200 || status >= 300) return HTTP_AUTH_ERROR;
    if (!name && !email) return HTTP_AUTH_OK;
    return http_parse_profile(response, name, name_size, email, email_size) ? HTTP_AUTH_ERROR : HTTP_AUTH_OK;
}

int http_auth_delete(const http_client *client) {
    char url[2048], body[1024], response[1024]; long status;
    snprintf(url, sizeof(url), "%s/api/0/auth", client->base_url);
    snprintf(body, sizeof(body), "{\"device_id\":\"%s\"}", client->device_id);
    if (request(client, url, "DELETE", body, response, sizeof(response), &status)) return -1;
    return status >= 200 && status < 300 ? 0 : -1;
}
