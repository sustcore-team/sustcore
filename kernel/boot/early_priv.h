/**
 * @file early_priv.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief BSP 早期启动内部接口
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <boot/boot.h>

#include <cstddef>

namespace boot::early_internal {
    void clear_bss() noexcept;
    void run_initializers() noexcept;
    void validate_bootinfo(size_t bsp_hwid, const BootInfoHeader *header) noexcept;
}  // namespace boot::early_internal
