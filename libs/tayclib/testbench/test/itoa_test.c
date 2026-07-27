#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <tay/itoa.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void check_result(char *result, char *buffer, const char *expected) {
    CHECK(result == buffer);
    CHECK(strcmp(buffer, expected) == 0);
}

int main(void) {
    char buffer[128];
    check_result(itoa_s(0, buffer, sizeof(buffer), 10), buffer, "0");
    check_result(itoa_s(42, buffer, sizeof(buffer), 2), buffer, "101010");
    check_result(itoa_s(-42, buffer, sizeof(buffer), 8), buffer, "-52");
    check_result(itoa_s(INT_MIN, buffer, sizeof(buffer), 10), buffer, "-2147483648");
    check_result(utoa_s(UINT_MAX, buffer, sizeof(buffer), 16), buffer, "ffffffff");
    check_result(ltoa_s(LONG_MIN, buffer, sizeof(buffer), 10), buffer,
                 sizeof(long) == 8 ? "-9223372036854775808" : "-2147483648");
    check_result(ultoa_s(35ul, buffer, sizeof(buffer), 36), buffer, "z");
    check_result(lltoa_s(LLONG_MIN, buffer, sizeof(buffer), 10), buffer,
                 "-9223372036854775808");
    check_result(ulltoa_s(ULLONG_MAX, buffer, sizeof(buffer), 16), buffer,
                 "ffffffffffffffff");

    memset(buffer, 'x', sizeof(buffer));
    CHECK(itoa_s(12345, buffer, 4, 10) == buffer);
    CHECK(memcmp(buffer, "123\0", 4) == 0);
    memset(buffer, 'x', sizeof(buffer));
    CHECK(itoa_s(-12, buffer, 2, 10) == buffer);
    CHECK(buffer[0] == '-' && buffer[1] == '\0');
    buffer[0] = 'q';
    CHECK(itoa_s(1, buffer, sizeof(buffer), 1) == buffer && buffer[0] == 'q');
    CHECK(itoa_s(1, buffer, sizeof(buffer), 37) == buffer && buffer[0] == 'q');
    CHECK(itoa_s(1, NULL, sizeof(buffer), 10) == NULL);
    buffer[0] = 'q';
    CHECK(itoa_s(1, buffer, 0, 10) == buffer && buffer[0] == 'q');

    return failures != 0;
}
