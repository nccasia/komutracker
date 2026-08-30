#include "afk.h"
#include <assert.h>
#include <string.h>

int main(void) {
    afk_sample sample = afk_update(false, 10.0, 180.0);
    assert(!sample.afk && !sample.changed && sample.duration == 0.0);

    sample = afk_update(false, 181.0, 180.0);
    assert(sample.afk && sample.changed && sample.duration == 181.0);

    sample = afk_update(true, 1.0, 180.0);
    assert(!sample.afk && sample.changed);

    char timestamp[32];
    assert(afk_format_timestamp(timestamp, sizeof(timestamp), 0.001) == 0);
    assert(strcmp(timestamp, "1970-01-01T00:00:00.001Z") == 0);
    return 0;
}
