#include "http.h"
#include <assert.h>
#include <string.h>

int main(void) {
    char value[128];
    assert(http_parse_json_string("null", value, sizeof(value)) == HTTP_AUTH_PENDING);
    assert(http_parse_json_string("\"token-value\"", value, sizeof(value)) == HTTP_AUTH_OK);
    assert(strcmp(value, "token-value") == 0);

    char name[128], email[128];
    assert(http_parse_profile("{\"name\":\"Test User\",\"email\":\"test@example.com\"}",
                              name, sizeof(name), email, sizeof(email)) == 0);
    assert(strcmp(name, "Test User") == 0);
    assert(strcmp(email, "test@example.com") == 0);
    return 0;
}
