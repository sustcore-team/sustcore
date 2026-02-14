/**
 * @file assert.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * theflysong(song_of_the_fly@163.com)
 * @brief SLUB Allocator
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kio.h>
#include <cstdarg>

extern "C" void assertion_failure(const char *expression, const char *file,
                                  const char *base_file, int line) {
    LOGGER::ERROR("assertion failed: %s (%s:%d, base=%s)", expression, file,
                 line, base_file);
    while (true);
}

extern "C" void panic_failure(const char *expression, const char *file,
                              const char *base_file, int line) {
    LOGGER::ERROR("panic_assert failed: %s (%s:%d, base=%s)", expression, file,
                 line, base_file);
    while (true);
}

extern "C" void panic(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vbprintf<KernelIO>(format, args);
    va_end(args);
    kputs("\n");
    while (true);
}
