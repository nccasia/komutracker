#include "auth.h"
#include "dirs.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <bcrypt.h>
#include <shellapi.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static int read_file(const char *path, char *out, size_t size) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    size_t count = fread(out, 1, size - 1, file);
    fclose(file);
    out[count] = '\0';
    while (count && (out[count - 1] == '\n' || out[count - 1] == '\r')) out[--count] = '\0';
    return count ? 0 : -1;
}

static int write_private(const char *path, const char *value) {
    if (dirs_create_parent(path)) return -1;
#ifdef _WIN32
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
#else
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    FILE *file = fdopen(fd, "wb");
    if (!file) { close(fd); return -1; }
#endif
    int result = fputs(value, file) == EOF ? -1 : 0;
    if (fclose(file) != 0) result = -1;
#ifndef _WIN32
    chmod(path, 0600);
#endif
    return result;
}

static int random_bytes(unsigned char *out, size_t size) {
#ifdef _WIN32
    return BCryptGenRandom(NULL, out, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 ? 0 : -1;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;
    size_t done = 0;
    while (done < size) {
        ssize_t count = read(fd, out + done, size - done);
        if (count <= 0) { close(fd); return -1; }
        done += (size_t)count;
    }
    close(fd);
    return 0;
#endif
}

int auth_get_device_id(char *out, size_t size) {
    if (size < 65) return -1;
    char path[2048];
    if (dirs_device_path(path, sizeof(path))) return -1;
    if (!read_file(path, out, size) && strlen(out) == 64) return 0;
    unsigned char random[32];
    if (random_bytes(random, sizeof(random))) return -1;
    for (size_t i = 0; i < sizeof(random); i++) sprintf(out + i * 2, "%02x", random[i]);
    out[64] = '\0';
    return write_private(path, out);
}

int auth_read_token(char *out, size_t size) {
    char path[2048];
    return dirs_token_path(path, sizeof(path)) ? -1 : read_file(path, out, size);
}

int auth_save_token(const char *token) {
    char path[2048];
    return dirs_token_path(path, sizeof(path)) ? -1 : write_private(path, token);
}

int auth_remove_token(void) {
    char path[2048];
    if (dirs_token_path(path, sizeof(path))) return -1;
    return remove(path) == 0 ? 0 : -1;
}

int auth_build_url(char *out, size_t size, const auth_options *options, const char *device_id) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    char *client = curl_easy_escape(curl, options->client_id, 0);
    char *redirect = curl_easy_escape(curl, options->redirect_uri, 0);
    char *state = curl_easy_escape(curl, device_id, 0);
    if (!client || !redirect || !state) {
        curl_free(client); curl_free(redirect); curl_free(state); curl_easy_cleanup(curl);
        return -1;
    }
    int count = snprintf(out, size,
        "%s/oauth2/auth?client_id=%s&redirect_uri=%s&response_type=code&scope=openid%%20offline&state=%s",
        options->auth_url, client, redirect, state);
    curl_free(client); curl_free(redirect); curl_free(state); curl_easy_cleanup(curl);
    return count >= 0 && count < (int)size ? 0 : -1;
}

int auth_open_browser(const char *url) {
#ifdef _WIN32
    return (INT_PTR)ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL) > 32 ? 0 : -1;
#else
#ifndef __APPLE__
    if ((!getenv("DISPLAY") || !*getenv("DISPLAY")) &&
        (!getenv("WAYLAND_DISPLAY") || !*getenv("WAYLAND_DISPLAY"))) return -1;
#endif
    pid_t child = fork();
    if (child < 0) return -1;
    if (child == 0) {
#ifdef __APPLE__
        execlp("open", "open", url, (char *)NULL);
#else
        execlp("xdg-open", "xdg-open", url, (char *)NULL);
#endif
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0) return -1;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
#endif
}

int auth_login(http_client *client, const auth_options *options, char *token, size_t token_size,
               volatile sig_atomic_t *running) {
    char url[4096];
    if (auth_build_url(url, sizeof(url), options, client->device_id)) return -1;
    printf("Open this URL to log in:\n%s\n", url);
    fflush(stdout);
    if (options->open_browser && auth_open_browser(url))
        fprintf(stderr, "Unable to open a browser; open the URL manually\n");

    int waited = 0;
    int attempt = 0;
    while (*running && waited < options->timeout_seconds) {
        attempt++;
        int result = http_auth_poll(client, token, token_size);
        if (result == HTTP_AUTH_OK) {
            fprintf(stderr, "komutracker %s: authentication poll attempt %d succeeded\n",
                    KOMUTRACKER_VERSION, attempt);
            if (auth_save_token(token)) return -1;
            client->token = token;
            return http_auth_me(client, NULL, 0, NULL, 0) == HTTP_AUTH_OK ? 0 : -1;
        }
        if (result == HTTP_AUTH_ERROR) {
            fprintf(stderr, "komutracker %s: authentication poll failed\n", KOMUTRACKER_VERSION);
            return -1;
        }
        fprintf(stderr, "komutracker %s: authentication poll attempt %d pending\n",
                KOMUTRACKER_VERSION, attempt);
        for (int tenth = 0; tenth < 20 && *running; tenth++) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100000);
#endif
        }
        waited += 2;
    }
    fprintf(stderr, *running ? "Login timed out\n" : "Login cancelled\n");
    return -1;
}
