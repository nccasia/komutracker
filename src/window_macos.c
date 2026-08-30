#ifdef __APPLE__
#include "window.h"
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libproc.h>
#include <string.h>

static void copy_cf(char *out, size_t size, CFStringRef value) {
    if (!value || !CFStringGetCString(value, out, size, kCFStringEncodingUTF8)) out[0] = '\0';
}

int window_init(void) { return 0; }

int window_get_current(window_info *out) {
    strcpy(out->app, "unknown"); out->title[0] = '\0';
    CFArrayRef windows = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!windows) return -1;
    CFIndex count = CFArrayGetCount(windows);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef window = CFArrayGetValueAtIndex(windows, i);
        CFNumberRef layer = CFDictionaryGetValue(window, kCGWindowLayer);
        int layer_value = -1;
        if (!layer || !CFNumberGetValue(layer, kCFNumberIntType, &layer_value) || layer_value != 0) continue;
        CFStringRef owner = CFDictionaryGetValue(window, kCGWindowOwnerName);
        copy_cf(out->app, sizeof(out->app), owner);
        CFStringRef title = CFDictionaryGetValue(window, kCGWindowName);
        copy_cf(out->title, sizeof(out->title), title);
        break;
    }
    CFRelease(windows);
    return strcmp(out->app, "unknown") ? 0 : -1;
}

void window_cleanup(void) {}
#endif
