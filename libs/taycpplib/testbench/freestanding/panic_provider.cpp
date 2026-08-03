/**
 * @file panic_provider.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 为 freestanding panic 链接测试提供 tay::panic 实现。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/panic.h>

[[noreturn]] void tay::panic(const char *) noexcept {
    for (;;) {
    }
}
