/**
 * @file stdio.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief linux subsystem libc stdio implementation
 * @version alpha-1.0.0
 * @date 2026-06-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <std/stdio.h>
#include <syscall.h>

#include <cstdarg>
#include <cstddef>
#include <cstring>

namespace {
    int serial_write_chunk(const char *data, size_t len, void *) {
        sys_write_serial(0, data, len);
        return static_cast<int>(len);
    }
}  // namespace

extern "C" int puts(const char *str) {
    static constexpr char NULL_STR[] = "(null)";

    const char *out = str != nullptr ? str : NULL_STR;
    size_t len      = strlen(out);
    sys_write_serial(0, out, len);
    sys_write_serial(0, "\n", 1);
    return static_cast<int>(len + 1);
}

extern "C" int printf(const char *fmt, ...) {
    char chunk[256];
    va_list args;
    va_start(args, fmt);
    int ret =
        vcbprintf(chunk, sizeof(chunk), serial_write_chunk, nullptr, fmt, args);
    va_end(args);
    return ret;

    return 0;
}
