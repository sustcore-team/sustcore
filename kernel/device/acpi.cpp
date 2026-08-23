/**
 * @file acpi.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ACPI 固件枚举后端实现
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/acpi.h>

namespace device::acpi {
    tay::expected<void, tay::error_code> Enumerator::enumerate(CatalogBuilder &,
                                                               FwInput input) noexcept {
        // BootInfo 扩展为 tagged firmware blob、RSDP 校验和静态表解析完成后再启用。
        (void)input;
        return tay::Err(tay::error_code::INVALID_ARGUMENT);
    }
}  // namespace device::acpi
