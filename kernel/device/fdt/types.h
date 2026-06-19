/**
 * @file types.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 通用类型声明
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/device.h>
#include <driver/base.h>
#include <sus/owner.h>
#include <sus/types.h>

#include <string>
#include <unordered_map>

namespace fdt {
    using driver::domain_t;
    using driver::DriverBase;
    using driver::hwirq_t;
    using driver::intc_t;

    using phandle_t = b32;

    constexpr const char *COMPATIBLE_PROP  = "compatible";
    constexpr const char *DEVICE_TYPE_PROP = "device_type";
    constexpr const char *STATUS_PROP      = "status";
    constexpr const char *OKAY_STATUS      = "okay";
    constexpr const char *OK_STATUS        = "ok";
    constexpr const char *DISABLED_STATUS  = "disabled";

    struct RegionCells {
        size_t addr_cells;
        size_t size_cells;
    };

    using LocalInterruptTargetMap =
        std::unordered_map<phandle_t, device::cpuid_t>;

    struct CpuIntcDescriptor {
        const struct Node *node = nullptr;
        device::cpuid_t hart_id = 0;
        intc_t identifier       = driver::INVALID_ICTRL_ID;
        std::string name;
    };
}  // namespace fdt
