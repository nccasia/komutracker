#ifdef __APPLE__
#include "idle.h"
#include <ApplicationServices/ApplicationServices.h>

int idle_init(void) { return 0; }

double idle_seconds(void) {
    return CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState,
                                                  kCGAnyInputEventType);
}

void idle_cleanup(void) {}
#endif
