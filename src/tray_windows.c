#ifdef _WIN32

#include "desktop.h"
#include "tray.h"

#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

#define IDI_KOMUTRACKER_APP 101
#define IDI_KOMUTRACKER_TRAY 102
#define WM_KOMUTRACKER_TRAY (WM_APP + 42)
#define MENU_DASHBOARD 2001
#define MENU_AUTH 2002
#define MENU_LOGOUT 2003
#define MENU_QUIT 2004

static const wchar_t *WINDOW_CLASS = L"KomuTrackerTrayWindow";
static tray_callbacks callbacks;
static tray_view current_view;
static HWND window_handle;
static NOTIFYICONDATAW notify_icon;
static UINT taskbar_created;

static void utf8_to_wide(const char *source, wchar_t *target, int target_size) {
    if (!source || MultiByteToWideChar(CP_UTF8, 0, source, -1, target, target_size) <= 0) {
        if (target_size) target[0] = L'\0';
    }
}

static void add_notify_icon(void) {
    notify_icon.cbSize = sizeof(notify_icon);
    notify_icon.hWnd = window_handle;
    notify_icon.uID = 1;
    notify_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    notify_icon.uCallbackMessage = WM_KOMUTRACKER_TRAY;
    notify_icon.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_KOMUTRACKER_TRAY));
    wcscpy_s(notify_icon.szTip, ARRAYSIZE(notify_icon.szTip), L"KomuTracker");
    Shell_NotifyIconW(NIM_ADD, &notify_icon);
    notify_icon.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &notify_icon);
}

static void show_menu(void) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    wchar_t account[512], status[256], today[128];
    utf8_to_wide(current_view.account_name, account, ARRAYSIZE(account));
    utf8_to_wide(current_view.status_text, status, ARRAYSIZE(status));
    AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, account);
    if (current_view.logged_in && current_view.today_time[0]) {
        utf8_to_wide(current_view.today_time, today, ARRAYSIZE(today));
        AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, today);
    }
    AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, status);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | (current_view.logged_in ? 0 : MF_GRAYED),
                MENU_DASHBOARD, L"Open Dashboard");
    if (current_view.status == TRAY_AUTHENTICATING)
        AppendMenuW(menu, MF_STRING, MENU_AUTH, L"Cancel Login");
    else if (!current_view.logged_in)
        AppendMenuW(menu, MF_STRING, MENU_AUTH, L"Log In");
    else
        AppendMenuW(menu, MF_STRING, MENU_LOGOUT, L"Log Out");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, MENU_QUIT, L"Quit KomuTracker");

    POINT position;
    GetCursorPos(&position);
    SetForegroundWindow(window_handle);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   position.x, position.y, 0, window_handle, NULL);
    PostMessageW(window_handle, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static LRESULT CALLBACK tray_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == taskbar_created) {
        add_notify_icon();
        return 0;
    }
    if (message == WM_KOMUTRACKER_TRAY) {
        UINT event = LOWORD(lparam);
        if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP) show_menu();
        else if (event == WM_LBUTTONDBLCLK && current_view.logged_in && callbacks.open_dashboard)
            callbacks.open_dashboard(callbacks.context);
        return 0;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(wparam)) {
            case MENU_DASHBOARD:
                if (callbacks.open_dashboard) callbacks.open_dashboard(callbacks.context);
                break;
            case MENU_AUTH:
                if (callbacks.auth_action) callbacks.auth_action(callbacks.context);
                break;
            case MENU_LOGOUT:
                if (callbacks.logout) callbacks.logout(callbacks.context);
                break;
            case MENU_QUIT:
                if (callbacks.quit) callbacks.quit(callbacks.context);
                break;
        }
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int tray_init(const tray_callbacks *provided_callbacks) {
    callbacks = *provided_callbacks;
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW window_class = {0};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = tray_window_proc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_KOMUTRACKER_APP));
    window_class.lpszClassName = WINDOW_CLASS;
    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return -1;

    window_handle = CreateWindowExW(0, WINDOW_CLASS, L"KomuTracker", 0,
                                    0, 0, 0, 0, HWND_MESSAGE, NULL, instance, NULL);
    if (!window_handle) return -1;
    taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    add_notify_icon();
    return 0;
}

void tray_update(const tray_view *view) {
    current_view = *view;
    wchar_t account[64], status[48];
    utf8_to_wide(view->account_name, account, ARRAYSIZE(account));
    utf8_to_wide(view->status_text, status, ARRAYSIZE(status));
    if (view->logged_in && view->today_time[0]) {
        wchar_t today[48];
        utf8_to_wide(view->today_time, today, ARRAYSIZE(today));
        _snwprintf_s(notify_icon.szTip, ARRAYSIZE(notify_icon.szTip), _TRUNCATE,
                     L"KomuTracker\n%s\n%s (%s)", account, today, status);
    } else {
        _snwprintf_s(notify_icon.szTip, ARRAYSIZE(notify_icon.szTip), _TRUNCATE,
                     L"KomuTracker\n%s\n%s", account, status);
    }
    notify_icon.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &notify_icon);
}

void tray_poll(void) {
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void tray_cleanup(void) {
    Shell_NotifyIconW(NIM_DELETE, &notify_icon);
    if (window_handle) DestroyWindow(window_handle);
    window_handle = NULL;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show_command) {
    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show_command;
    return komutracker_desktop_main();
}

#endif
