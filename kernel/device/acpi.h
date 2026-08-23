/**
 * @file acpi.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ACPI 固件枚举后端合同。
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 *
 * 当前启动 ABI 尚未传递 ACPI_RSDP；实现只保留后端边界，避免把 FDT 专用接口扩散到
 * ACPI。接入静态表解析后，Enumerator 应复用同一个 CatalogBuilder。
 */

#pragma once

#include <device/catalog.h>

namespace device::acpi {
    class Enumerator final : public FwEnumerator {
    public:
        [[nodiscard]] tay::expected<void, tay::error_code> enumerate(
            CatalogBuilder &, FwInput input) noexcept override;
    };
}  // namespace device::acpi
