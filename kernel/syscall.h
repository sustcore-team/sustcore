/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核最小系统调用实现接口。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/expected.h>

#include <cstddef>

namespace kernel::syscall {
    inline constexpr size_t EC_WRITE_SYSCALL = 1;
    inline constexpr size_t YIELD_SYSCALL    = 2;

    [[nodiscard]] tay::expected<size_t, tay::error_code> ec_write(const char *data,
                                                                  size_t length) noexcept;
    void yield() noexcept;
}  // namespace kernel::syscall
