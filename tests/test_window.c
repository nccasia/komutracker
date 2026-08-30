#include "http.h"
#include <assert.h>
#include <string.h>

int main(void) {
    char escaped[256];
    assert(http_json_escape("Code \"file\"\\main\nnext\t", escaped, sizeof(escaped)) == 0);
    assert(strcmp(escaped, "Code \\\"file\\\"\\\\main\\nnext\\t") == 0);
    assert(http_json_escape("日本語", escaped, sizeof(escaped)) == 0);
    assert(strcmp(escaped, "日本語") == 0);
    char small[2];
    assert(http_json_escape("too long", small, sizeof(small)) == -1);
    return 0;
}
