/**
 * @file panic.cpp
 * @brief Native host implementation of tay::panic.
 * @version 0.1.0-dev.1
 * @date 2026-07-27
 */

#include <cstdio>
#include <cstdlib>

#include <tay/panic.h>

[[noreturn]] void tay::panic(const char *message) noexcept {
    std::fputs("tay panic: ", stderr);
    std::fputs(message != nullptr ? message : "<null>", stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
    std::abort();
}
