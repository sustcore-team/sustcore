/**
 * @file kio.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kernel I/O implementation
 * @version alpha-1.0.0
 * @date 2026-04-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <logger.h>
#include <arch/description.h>
#include <sustcore/addr.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
    int serial_write_chunk(const char *data, size_t len, void *) {
        EarlySerial::serial_write_string(len, data);
        return len;
    }

    int kvprintf(const char *fmt, va_list args) {
        char chunk[256];
        return vcbprintf(chunk, sizeof(chunk), serial_write_chunk, nullptr, fmt,
                         args);
    }
}  // namespace

int kputs(const char *str) {
    size_t len        = strlen(str);
    EarlySerial::serial_write_string(len, str);
    return strlen(str);
}

int sys_write_serial(const char *str, size_t len)
{
    EarlySerial::serial_write_string(len, str);
    return len;
}

int kputchar(char ch) {
    EarlySerial::serial_write_char(ch);
    return ch;
}

char kgetchar() {
    return '\0';
}

int kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = kvprintf(fmt, args);
    va_end(args);
    return len;
}

int kprintfln(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = kvprintf(fmt, args);
    va_end(args);
    kputchar('\n');
    return len + 1;
}
