#ifdef _WIN32
#include "window.h"
#include <windows.h>
#include <string.h>

static void utf8(char *out, int size, const wchar_t *value) {
    if (!WideCharToMultiByte(CP_UTF8, 0, value, -1, out, size, NULL, NULL)) out[0] = '\0';
}

int window_init(void) { return 0; }

int window_get_current(window_info *out) {
    strcpy(out->app, "unknown"); out->title[0] = '\0';
    HWND window = GetForegroundWindow();
    if (!window) return -1;
    wchar_t title[WINDOW_TITLE_SIZE];
    if (GetWindowTextW(window, title, WINDOW_TITLE_SIZE)) utf8(out->title, WINDOW_TITLE_SIZE, title);
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return 0;
    wchar_t path[32768]; DWORD length = 32768;
    if (QueryFullProcessImageNameW(process, 0, path, &length)) {
        wchar_t *base = wcsrchr(path, L'\\');
        utf8(out->app, WINDOW_APP_SIZE, base ? base + 1 : path);
    }
    CloseHandle(process);
    return 0;
}
void window_cleanup(void) {}
#endif
