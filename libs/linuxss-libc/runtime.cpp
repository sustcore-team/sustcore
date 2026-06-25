/**
 * @file runtime.cpp
 * @author theflysong
 * @brief linux subsystem libc language runtime support
 * @version alpha-1.0.0
 * @date 2026-06-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <std/assert.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <syscall.h>

#include <cstdarg>
#include <cstddef>

void *operator new(size_t size) {
    void *ptr = malloc(size);
    if (ptr == nullptr) {
        panic("operator new failed size=%lu", static_cast<unsigned long>(size));
    }
    return ptr;
}

void operator delete(void *ptr) noexcept {
    free(ptr);
}

void *operator new[](size_t size) {
    void *ptr = malloc(size);
    if (ptr == nullptr) {
        panic("operator new[] failed size=%lu",
              static_cast<unsigned long>(size));
    }
    return ptr;
}

void operator delete[](void *ptr) noexcept {
    free(ptr);
}

void operator delete(void *ptr, size_t) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, size_t) noexcept {
    free(ptr);
}

namespace {
    int panic_write_chunk(const char *data, size_t len, void *) {
        sys_write_serial(0, data, len);
        return static_cast<int>(len);
    }
}  // namespace

extern "C" {
int __cxa_atexit(void (*)(void *), void *, void *) {
    return 0;
}

void panic(const char *format, ...) {
    char chunk[256];
    va_list args;
    va_start(args, format);
    sys_write_serial(0, "panic: ", 7);
    vcbprintf(chunk, sizeof(chunk), panic_write_chunk, nullptr,
              format == nullptr ? "(null)" : format, args);
    va_end(args);
    sys_write_serial(0, "\n", 1);
    while (true) {
    }
}
}
