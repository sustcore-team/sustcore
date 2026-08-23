/**
 * @file fw.h
 * @brief 定义固件目录跨错误与目录模型共享的稳定标识类型。
 */

#pragma once

#include <tay/bits.h>

namespace device {
    enum class FwKind : u8_t {
        NONE,
        FDT,
        ACPI,
        RUNTIME_BUS,
    };

    struct FwId {
        FwKind kind        = FwKind::NONE;
        u64_t namespace_id = 0;
        u64_t local_id     = 0;

        [[nodiscard]] constexpr bool valid() const noexcept {
            return kind != FwKind::NONE;
        }
        [[nodiscard]] friend constexpr bool operator==(FwId left, FwId right) noexcept = default;
    };
}  // namespace device
