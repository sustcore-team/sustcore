/**
 * @file itoa.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 实现 Tay C 库的整数到 ASCII 字符串转换例程。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <assert.h>
#include <stdbool.h>
#include <tay/attribute.h>
#include <tay/itoa.h>

static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static ALWAYS_INLINE void __set_s(char *buf, size_t bufsz, size_t pos, char c) {
    // if the pos will exceed the buffer size(bufsz - 1, the final byte is
    // reserved for '\0'), then return directly
    if (pos >= bufsz - 1) {
        return;
    }
    buf[pos] = c;
}

static ALWAYS_INLINE void __set0(char *buf, size_t bufsz, size_t pos) {
    if (pos >= bufsz) {
        pos = bufsz - 1;
    }
    buf[pos] = '\0';
}

static char *__itoa_convert_s(unsigned long long val, bool negative, char *buf, size_t bufsz,
                              int radix) {
    if (buf == NULL || bufsz == 0 || radix < 2 || radix > 36) {
        return buf;
    }

    size_t pos = 0;

    if (negative) {
        __set_s(buf, bufsz, pos++, '-');
    }

    if (val == 0) {
        __set_s(buf, bufsz, pos++, '0');
        __set0(buf, bufsz, pos++);
        return buf;
    }

    // calculate the number of digits
    unsigned long long tmp = val;
    size_t num_digits      = 0;
    while (tmp > 0) {
        tmp /= (unsigned int)radix;
        num_digits++;
    }

    // fill the buffer in reverse order
    size_t fin_digit = pos + num_digits - 1;
    while (val > 0) {
        unsigned int digit = val % (unsigned int)radix;
        __set_s(buf, bufsz, fin_digit--, digits[digit]);
        val /= (unsigned int)radix;
    }

    assert(fin_digit + 1 == pos);          // ensure we filled the correct number of digits
    __set0(buf, bufsz, pos + num_digits);  // set the tailing null terminator
    return buf;
}

char *itoa_s(int val, char *buf, size_t bufsz, int radix) {
    if (val < 0) {
        // Avoid overflow when val is INT_MIN.
        unsigned int uval = (unsigned int)(-(val + 1)) + 1u;
        return __itoa_convert_s(uval, true, buf, bufsz, radix);
    }
    return __itoa_convert_s((unsigned int)val, false, buf, bufsz, radix);
}

char *utoa_s(unsigned int val, char *buf, size_t bufsz, int radix) {
    return __itoa_convert_s(val, false, buf, bufsz, radix);
}

char *ltoa_s(long val, char *buf, size_t bufsz, int radix) {
    if (val < 0) {
        // Avoid overflow when val is LONG_MIN.
        unsigned long uval = (unsigned long)(-(val + 1)) + 1ul;
        return __itoa_convert_s(uval, true, buf, bufsz, radix);
    }
    return __itoa_convert_s((unsigned long)val, false, buf, bufsz, radix);
}

char *ultoa_s(unsigned long val, char *buf, size_t bufsz, int radix) {
    return __itoa_convert_s(val, false, buf, bufsz, radix);
}

char *lltoa_s(long long val, char *buf, size_t bufsz, int radix) {
    if (val < 0) {
        // Avoid overflow when val is LLONG_MIN.
        unsigned long long uval = (unsigned long long)(-(val + 1)) + 1ull;
        return __itoa_convert_s(uval, true, buf, bufsz, radix);
    }
    return __itoa_convert_s((unsigned long long)val, false, buf, bufsz, radix);
}

char *ulltoa_s(unsigned long long val, char *buf, size_t bufsz, int radix) {
    return __itoa_convert_s(val, false, buf, bufsz, radix);
}
