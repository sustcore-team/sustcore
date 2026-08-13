/**
 * @file page_flags.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 架构无关的页映射属性与查询结果
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <sustcore/addr.h>
#include <tay/bits.h>

namespace memory {
    enum class FaultAccess : u8_t {
        NONE,
        READ,
        WRITE,
        EXECUTE,
    };

    enum class CacheMode : u8_t {
        NORMAL,
        DEVICE,
    };

    struct PageFlags {
        bool readable   = true;
        bool writable   = false;
        bool executable = false;
        bool user       = false;
        bool global     = false;
        CacheMode cache = CacheMode::NORMAL;

        [[nodiscard]] constexpr bool operator==(const PageFlags &) const noexcept = default;
    };

    struct PageMapping {
        PhyAddr physical{};
        PageFlags flags{};
    };
}  // namespace memory
