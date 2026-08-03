/**
 * @file boot.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 启动器与内核之间的启动信息 ABI
 * @version 0.1.0-dev.1
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addr.h>
#include <tay/bits.h>
#include <tay/format.h>

#include <cstddef>

constexpr size_t MAX_BOOTINFO_SIZE    = 128 * 1024;
constexpr size_t MAX_BOOTINFO_REGIONS = 128;

/** @brief 物理内存区域的启动期生命周期类别。 */
enum class MemoryType : u32_t {
    FREE             = 0,
    RESERVED         = 1,
    BOOT_RECLAIMABLE = 2,
    ACPI_RECLAIMABLE = 3,
    ACPI_NVS         = 4,
    IOMMU            = 5,
    BAD_MEMORY       = 6,
};

/**
 * @brief 启动器传递给内核的物理内存父区域或保留子区域。
 *
 * 所有 FREE 父区域位于数组前缀。父区域的 rsvd_idx 指向首个子区域，子区域的
 * rsvd_idx 指向同一父区域中的下一个子区域；链尾和无子区域的父区域指向自身。
 */
struct MemoryRegion {
    PhyArea area;
    MemoryType type;
    size_t rsvd_idx;

    /** @brief 返回内存区域状态的稳定日志名称。 */
    [[nodiscard]] static constexpr const char *type_name(MemoryType value) noexcept {
        switch (value) {
            case MemoryType::FREE:             return "可用";
            case MemoryType::RESERVED:         return "保留";
            case MemoryType::BOOT_RECLAIMABLE: return "启动后可回收";
            case MemoryType::ACPI_RECLAIMABLE: return "ACPI 可回收";
            case MemoryType::ACPI_NVS:         return "ACPI NVS";
            case MemoryType::IOMMU:            return "IOMMU";
            case MemoryType::BAD_MEMORY:       return "坏内存";
        }
        return "未知";
    }
};

static_assert(sizeof(MemoryType) == sizeof(u32_t));
static_assert(offsetof(MemoryRegion, area) == 0);
static_assert(offsetof(MemoryRegion, type) == sizeof(PhyArea));
static_assert(sizeof(MemoryRegion) == 32);

namespace tay {
    template <>
    struct formatter<MemoryRegion> {
        constexpr format_parse_context::iterator parse(format_parse_context &context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const MemoryRegion &region,
                                                FormatContext &context) const {
            context.write("内存区域{范围=[");
            context.format("{}", region.area.begin.addr());
            context.write(", ");
            context.format("{}", region.area.end.addr());
            context.write("), 类型=");
            context.write(MemoryRegion::type_name(region.type));
            context.write(", 保留索引=");
            context.format("{}", region.rsvd_idx);
            context.write("}");
            return context.out();
        }
    };
}  // namespace tay

/**
 * @brief BootInfo 固定头；MemoryRegion 数组和 extras 紧随其后。
 * @note 这是启动器与内核共享的 wire ABI，消费者必须先校验 info_sz 和 region_cnt。
 */
struct BootInfoHeader {
    // 启动器传递的完整对象字节数，含 regions 与 extras。
    size_t info_sz;
    // BSP 的硬件 ID; 不是逻辑 CPU 下标
    size_t hart_id;
    size_t region_cnt;
    // regions 紧随 header；extras 紧随 regions。当前 extras 的首项为 FDT 物理地址。
};

static_assert(offsetof(BootInfoHeader, info_sz) == 0);
static_assert(offsetof(BootInfoHeader, hart_id) == sizeof(size_t));

/** @brief 返回 BootInfo 尾随 MemoryRegion 数组的可写首地址。 */
static inline MemoryRegion *bootinfo_regions(BootInfoHeader *header) noexcept {
    auto *addr = reinterpret_cast<std::byte *>(header);
    return reinterpret_cast<MemoryRegion *>(addr + sizeof(BootInfoHeader));
}

/** @brief 返回 BootInfo 尾随 MemoryRegion 数组的只读首地址。 */
static inline const MemoryRegion *bootinfo_regions(const BootInfoHeader *header) noexcept {
    const auto *addr = reinterpret_cast<const std::byte *>(header);
    return reinterpret_cast<const MemoryRegion *>(addr + sizeof(BootInfoHeader));
}

/** @brief 返回 MemoryRegion 数组之后的可写 extras 首地址。 */
static inline void *bootinfo_extras(BootInfoHeader *header) noexcept {
    auto *addr = reinterpret_cast<std::byte *>(header);
    return addr + sizeof(BootInfoHeader) + sizeof(MemoryRegion) * header->region_cnt;
}

/** @brief 返回 MemoryRegion 数组之后的只读 extras 首地址。 */
static inline const void *bootinfo_extras(const BootInfoHeader *header) noexcept {
    const auto *addr = reinterpret_cast<const std::byte *>(header);
    return addr + sizeof(BootInfoHeader) + sizeof(MemoryRegion) * header->region_cnt;
}

/** @brief 返回 extras 中 FDT 物理地址槽位的可写指针。 */
static inline PhyAddr *bootinfo_fdt_pa(BootInfoHeader *header) noexcept {
    return reinterpret_cast<PhyAddr *>(bootinfo_extras(header));
}

/** @brief 返回 extras 中 FDT 物理地址槽位的只读指针。 */
static inline const PhyAddr *bootinfo_fdt_pa(const BootInfoHeader *header) noexcept {
    return reinterpret_cast<const PhyAddr *>(bootinfo_extras(header));
}

/** @brief 读取 BootInfo extras 保存的 FDT 物理地址。 */
static inline PhyAddr bootinfo_fdt(BootInfoHeader *header) noexcept {
    return *bootinfo_fdt_pa(header);
}

/** @brief 读取只读 BootInfo extras 保存的 FDT 物理地址。 */
static inline PhyAddr bootinfo_fdt(const BootInfoHeader *header) noexcept {
    return *bootinfo_fdt_pa(header);
}
