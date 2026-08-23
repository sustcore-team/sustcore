/**
 * @file pt_root.h
 * @brief 架构无关的地址空间根绑定描述。
 */

#pragma once

#include <sustcore/addr.h>
#include <tay/bits.h>

namespace memory {
    enum class RootRole : u8_t {
        KERNEL,
        CLIENT,
    };

    struct RootBinding final {
        /** @brief 当前地址空间绑定的私有（低半区）页表根。 */
        PhyAddr private_root{};
        u16_t asid    = 0;
        RootRole role = RootRole::KERNEL;
    };
}  // namespace memory
