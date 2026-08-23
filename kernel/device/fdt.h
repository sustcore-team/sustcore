/**
 * @file fdt.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 到设备目录的启动期枚举后端。
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/catalog.h>
#include <error/fdt.h>

namespace boot {
    struct Context;
}

namespace device::fdt {
    class Enumerator final : public FwEnumerator {
    public:
        [[nodiscard]] tay::expected<void, tay::error_code> enumerate(CatalogBuilder &,
                                                                     FwInput) noexcept override;
    };

    [[nodiscard]] tay::expected<void, FdtError> enumerate(CatalogBuilder &builder,
                                                          const boot::Context &context) noexcept;
}  // namespace device::fdt
