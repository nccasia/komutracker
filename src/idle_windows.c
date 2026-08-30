#ifdef _WIN32
#include "idle.h"
#include <windows.h>

int idle_init(void) { return 0; }

double idle_seconds(void) {
    LASTINPUTINFO info = { sizeof(info) };
    if (!GetLastInputInfo(&info)) return -1.0;
    return (double)(GetTickCount() - info.dwTime) / 1000.0;
}

void idle_cleanup(void) {}
#endif
