/**
 * @file boot.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 启动信息
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addr.h>
#include <sus/types.h>

struct MemRegion {
    PhyArea region;
    enum class MemoryStatus {
        FREE             = 0,
        RESERVED         = 1,
        // reclaimable 区域
        // 在彻底摆脱启动时环境时可回收
        // 例如内核页表, 已保存的启动参数等
        BOOT_RECLAIMABLE = 2,
        ACPI_RECLAIMABLE = 3,
        ACPI_NVS         = 4,
        BAD_MEMORY       = 5
    } status;
};

using BootInfoExtra = void;
struct BootInfoStruct {
    // 信息总大小
    size_t info_sz;
    size_t hart_id;
    // 当前核心号
    size_t region_cnt;
    MemRegion *regions;
    BootInfoExtra *extra;
};