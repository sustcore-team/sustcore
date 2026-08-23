/**
 * @file page_db.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 物理页角色、所有权与启动期区域目录
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <boot/boot.h>
#include <sustcore/addr.h>
#include <tay/bits.h>

#include <atomic>
#include <cstddef>

namespace memory {
    /** @brief 物理页当前承担的高层角色。 */
    enum class PageKind : u8_t {
        GENERIC,
        RESERVED,
        BOOT_RECLAIMABLE,
        ACPI_RECLAIMABLE,
        ACPI_NVS,
        IOMMU,
        BAD_MEMORY,
        METADATA,
        BOOT_DATA,
        PAGE_TABLE,
        KERNEL_HEAP,
        USER,
    };

    /** @brief 物理页相对于高层所有者的生命周期状态。 */
    enum class PageState : u8_t {
        AVAILABLE,
        RESERVED,
        CLAIMED,
    };

    /** @brief 每个 base page 对应的固定大小高层元数据。 */
    struct alignas(8) PageDesc {
        std::atomic<u32_t> references{0};
        std::atomic<u32_t> map_count{0};
        u64_t owner_id   = 0;
        u64_t auxiliary  = 0;
        u32_t flags      = 0;
        u16_t generation = 0;
        PageKind kind    = PageKind::GENERIC;
        PageState state  = PageState::AVAILABLE;
    };

    inline constexpr size_t PAGE_DESCRIPTOR_SIZE = 32;
    static_assert(sizeof(PageDesc) == PAGE_DESCRIPTOR_SIZE);
    static_assert(alignof(PageDesc) == 8);

    /** @brief 一个 FREE 父区域及其常驻 descriptor array。 */
    struct PhysRegion {
        PhyArea parent{};
        PhyArea metadata_backing{};
        PageDesc *descriptors    = nullptr;
        size_t page_count        = 0;
        size_t boot_region_index = 0;
    };

    /** @brief 分段式物理页事实数据库。 */
    class PageDb final {
    public:
        constexpr PageDb() noexcept = default;

        /** @brief 根据已校验的 BootInfo 放置并构造全部 descriptor array。 */
        void initialize(const BootInfoHeader &bootinfo) noexcept;

        /** @brief 查询物理地址所属的父区域；未受管理时返回 nullptr。 */
        [[nodiscard]] PhysRegion *find_region(PhyAddr address) noexcept;
        [[nodiscard]] const PhysRegion *find_region(PhyAddr address) const noexcept;

        /**
         * @brief 查询物理页 descriptor；地址必须页对齐且属于某个父区域。
         * @note ownership 字段由 Buddy claim/release 同步域保护。调用者必须持有对应
         *       Buddy 锁，或持有该页的独占所有权/生命周期 pin；map_count 与 references
         *       可使用各自的原子接口读取。
         */
        [[nodiscard]] PageDesc *lookup(PhyAddr address) noexcept;
        [[nodiscard]] const PageDesc *lookup(PhyAddr address) const noexcept;

        /** @brief 将 allocator 提供的页声明为指定高层角色。 */
        [[nodiscard]] bool claim(PhyArea area, PageKind kind, u64_t owner_id) noexcept;

        /** @brief 解除高层角色，使页面重新回到 allocator-managed 状态。 */
        void release(PhyArea area, PageKind kind, u64_t owner_id) noexcept;

        /** @brief 将启动期保留页转换为可发布给 Buddy 的普通页。 */
        void release_reclaim(PhyArea area) noexcept;

        [[nodiscard]] size_t region_count() const noexcept {
            return region_count_;
        }
        [[nodiscard]] const PhysRegion &region(size_t index) const noexcept;
        [[nodiscard]] bool initialized() const noexcept {
            return initialized_;
        }

    private:
        PhysRegion regions_[MAX_BOOTINFO_REGIONS]{};
        size_t region_count_ = 0;
        bool initialized_    = false;
    };

    /** @brief 获取唯一物理页数据库。 */
    [[nodiscard]] PageDb &page_db() noexcept;
}  // namespace memory
