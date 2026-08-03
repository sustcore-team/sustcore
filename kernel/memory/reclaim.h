/**
 * @file reclaim.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 最终页表启用后的内核初始化段回收
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstddef>

namespace memory {
    /** @brief 移除 .init 高地址映射并将其物理页归还 Buddy。 */
    [[nodiscard]] size_t reclaim_init_memory() noexcept;
}  // namespace memory
