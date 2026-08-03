/**
 * @file root_binding.h
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
        PhyAddr client_root{};
        u16_t asid    = 0;
        RootRole role = RootRole::KERNEL;
    };
}  // namespace memory
