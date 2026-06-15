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

#include <sus/types.h>
#include <sustcore/addr.h>

struct MemRegion {
    PhyAddr ptr;
    size_t size;
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

struct BootInfoHeader {
    // 信息总大小
    size_t info_sz;
    size_t hart_id;
    // 当前核心号
    size_t region_cnt;
    // regions 应该紧紧跟在 BootInfoHeader 之后
    // MemRegion regions[];
    // 跟在 regions 之后的应该是额外的 Boot 信息
    // FDT Blob;
};

static inline MemRegion *bootinfo_regions(BootInfoHeader *header) {
    char *_addr = reinterpret_cast<char *>(header);
    return reinterpret_cast<MemRegion *>(_addr + sizeof(BootInfoHeader));
}

static inline const MemRegion *bootinfo_regions(const BootInfoHeader *header) {
    return bootinfo_regions(const_cast<BootInfoHeader *>(header));
}

static inline void *bootinfo_extras(BootInfoHeader *header) {
    char *_addr = reinterpret_cast<char *>(header);
    return reinterpret_cast<void *>(_addr + sizeof(BootInfoHeader) +
                                     sizeof(MemRegion) * header->region_cnt);
}

static inline const void *bootinfo_extras(const BootInfoHeader *header) {
    return bootinfo_extras(const_cast<BootInfoHeader *>(header));
}

static inline void *bootinfo_fdt(BootInfoHeader *header) {
    return bootinfo_extras(header);
}

static inline const void *bootinfo_fdt(const BootInfoHeader *header) {
    return bootinfo_extras(header);
}
