/**
 * @file runtime.cpp
 * @author theflysong
 * @brief linux subsystem 运行时基础支持
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <prm.h>
#include <logger.h>
#include <std/assert.h>
#include <std/stdio.h>

#include <cstdarg>
#include <cstddef>

namespace {
    [[nodiscard]]
    void *linuxss_alloc(size_t size) {
        if (size == 0) {
            size = 1;
        }
        size_t old_brk = __linuxss_ss_brk;
        size_t new_brk = old_brk + size;
        if (new_brk < old_brk) {
            return nullptr;
        }
        size_t actual_brk = linuxss_brk(new_brk);
        if (actual_brk != new_brk) {
            return nullptr;
        }
        return reinterpret_cast<void *>(old_brk);
    }
}  // namespace

void *operator new(size_t size) {
    void *ptr = linuxss_alloc(size);
    if (ptr == nullptr) {
        panic("operator new failed size=%lu", static_cast<unsigned long>(size));
    }
    return ptr;
}

void operator delete(void *ptr) noexcept {
    (void)ptr;
}

void operator delete(void *ptr, size_t size) noexcept {
    (void)ptr;
    (void)size;
}

void *operator new[](size_t size) {
    void *ptr = linuxss_alloc(size);
    if (ptr == nullptr) {
        panic("operator new[] failed size=%lu", static_cast<unsigned long>(size));
    }
    return ptr;
}

void operator delete[](void *ptr) noexcept {
    (void)ptr;
}

void operator delete[](void *ptr, size_t size) noexcept {
    (void)ptr;
    (void)size;
}

extern "C" {
void *__dso_handle = nullptr;

int __cxa_atexit(void (*)(void *), void *, void *) {
    return 0;
}
}

void panic(const char *format, ...) {
    char buffer[256]{};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format == nullptr ? "(null)" : format,
              args);
    va_end(args);
    loggers::LXRT::FATAL("panic: %s", buffer);
    while (true) {
    }
}
