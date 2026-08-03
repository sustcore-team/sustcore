/**
 * @file panic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 实现 Tay C++ 库在本机环境中的 tay::panic 处理。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/panic.h>

#include <cstdio>
#include <cstdlib>

[[noreturn]] void tay::panic(const char *message) noexcept {
    std::fputs("tay panic: ", stderr);
    std::fputs(message != nullptr ? message : "<null>", stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
    std::abort();
}
