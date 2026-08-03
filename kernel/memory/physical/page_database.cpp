/**
 * @file page_database.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 物理页数据库构造与高层所有权转换
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <memory/physical/page_database.h>
#include <sustcore/addrspace.h>

#include <cstddef>
#include <new>

namespace memory {
    namespace {
        constinit PageDatabase database;

        [[nodiscard]] constexpr size_t pages_for_descriptors(size_t page_count) noexcept {
            constexpr size_t DESCRIPTORS_PER_PAGE = PAGE_SIZE / PAGE_DESCRIPTOR_SIZE;
            return (page_count + DESCRIPTORS_PER_PAGE - 1) / DESCRIPTORS_PER_PAGE;
        }

        [[nodiscard]] PageKind kind_for(MemoryType type) noexcept {
            switch (type) {
                case MemoryType::FREE:             return PageKind::GENERIC;
                case MemoryType::RESERVED:         return PageKind::RESERVED;
                case MemoryType::BOOT_RECLAIMABLE: return PageKind::BOOT_RECLAIMABLE;
                case MemoryType::ACPI_RECLAIMABLE: return PageKind::ACPI_RECLAIMABLE;
                case MemoryType::ACPI_NVS:         return PageKind::ACPI_NVS;
                case MemoryType::IOMMU:            return PageKind::IOMMU;
                case MemoryType::BAD_MEMORY:       return PageKind::BAD_MEMORY;
            }
            kernel::log::panic("构造 PageDatabase 时遇到无效内存类型");
        }

        void initialize_range(PageDescriptor *base, size_t first, size_t count, PageKind kind,
                              PageState state) noexcept {
            for (size_t index = 0; index < count; ++index) {
                auto &descriptor = base[first + index];
                descriptor.references.store(0, std::memory_order_relaxed);
                descriptor.map_count.store(0, std::memory_order_relaxed);
                descriptor.owner_id  = 0;
                descriptor.auxiliary = 0;
                descriptor.flags     = 0;
                descriptor.kind      = kind;
                descriptor.state     = state;
            }
        }
    }  // namespace

    PageDatabase &page_database() noexcept {
        return database;
    }

    void PageDatabase::initialize(const BootInfoHeader &bootinfo) noexcept {
        if (initialized_)
            kernel::log::panic("PageDatabase 被重复初始化");

        const auto *boot_regions = bootinfo_regions(&bootinfo);
        while (region_count_ < bootinfo.region_cnt &&
               boot_regions[region_count_].type == MemoryType::FREE)
            ++region_count_;
        if (region_count_ == 0 || region_count_ > MAX_BOOTINFO_REGIONS)
            kernel::log::panic("无效的 PageDatabase 父区域数量");

        for (size_t parent_idx = 0; parent_idx < region_count_; ++parent_idx) {
            const auto &parent          = boot_regions[parent_idx];
            const size_t page_count     = parent.area.size() / PAGE_SIZE;
            const size_t metadata_pages = pages_for_descriptors(page_count);
            if (metadata_pages == 0 || metadata_pages > addr_t(-1) / PAGE_SIZE)
                kernel::log::panic("无效的 PageDatabase 元数据大小");
            const size_t metadata_bytes = metadata_pages * PAGE_SIZE;

            addr_t cursor = parent.area.begin.arith();
            PhyArea backing{};
            size_t child_idx = parent.rsvd_idx;
            while (true) {
                const addr_t gap_end = child_idx == parent_idx
                                           ? parent.area.end.arith()
                                           : boot_regions[child_idx].area.begin.arith();
                if (cursor <= gap_end && metadata_bytes <= gap_end - cursor) {
                    backing = PhyArea(PhyAddr(cursor), PhyAddr(cursor + metadata_bytes));
                    break;
                }
                if (child_idx == parent_idx)
                    break;
                cursor            = boot_regions[child_idx].area.end.arith();
                const size_t next = boot_regions[child_idx].rsvd_idx;
                if (next == child_idx) {
                    child_idx = parent_idx;
                } else {
                    child_idx = next;
                }
            }
            if (backing.nullable())
                kernel::log::panic("FREE 父区域没有连续的元数据后备");

            auto *descriptors = reinterpret_cast<PageDescriptor *>(PA2KPA(backing.begin.arith()));
            for (size_t page = 0; page < page_count; ++page)
                new (&descriptors[page]) PageDescriptor{};

            auto &region             = regions_[parent_idx];
            region.parent            = parent.area;
            region.metadata_backing  = backing;
            region.descriptors       = descriptors;
            region.page_count        = page_count;
            region.boot_region_index = parent_idx;

            child_idx = parent.rsvd_idx;
            while (child_idx != parent_idx) {
                const auto &child  = boot_regions[child_idx];
                const size_t first = (child.area.begin - parent.area.begin) / PAGE_SIZE;
                const size_t count = child.area.size() / PAGE_SIZE;
                initialize_range(descriptors, first, count, kind_for(child.type),
                                 PageState::RESERVED);
                const size_t next = child.rsvd_idx;
                if (next == child_idx)
                    break;
                child_idx = next;
            }

            const size_t metadata_first = (backing.begin - parent.area.begin) / PAGE_SIZE;
            initialize_range(descriptors, metadata_first, metadata_pages, PageKind::METADATA,
                             PageState::RESERVED);
            kernel::log::info("物理区域 [{}]: {}, 共 {} 页; 元数据位于 {}, 共 {} 页", parent_idx,
                              parent.area, page_count, backing, metadata_pages);
        }
        initialized_ = true;
    }

    PhysicalMemoryRegion *PageDatabase::find_region(PhyAddr address) noexcept {
        for (size_t index = 0; index < region_count_; ++index)
            if (within(regions_[index].parent, address))
                return &regions_[index];
        return nullptr;
    }

    const PhysicalMemoryRegion *PageDatabase::find_region(PhyAddr address) const noexcept {
        for (size_t index = 0; index < region_count_; ++index)
            if (within(regions_[index].parent, address))
                return &regions_[index];
        return nullptr;
    }

    PageDescriptor *PageDatabase::lookup(PhyAddr address) noexcept {
        if (!address.aligned<PAGE_SIZE>())
            return nullptr;
        auto *owner = find_region(address);
        if (owner == nullptr)
            return nullptr;
        return &owner->descriptors[(address - owner->parent.begin) / PAGE_SIZE];
    }

    const PageDescriptor *PageDatabase::lookup(PhyAddr address) const noexcept {
        if (!address.aligned<PAGE_SIZE>())
            return nullptr;
        const auto *owner = find_region(address);
        if (owner == nullptr)
            return nullptr;
        return &owner->descriptors[(address - owner->parent.begin) / PAGE_SIZE];
    }

    bool PageDatabase::claim(PhyArea area, PageKind kind, u64_t owner_id) noexcept {
        if (area.nullable() || !area.begin.aligned<PAGE_SIZE>() || !area.end.aligned<PAGE_SIZE>() ||
            kind == PageKind::GENERIC)
            return false;
        for (addr_t current = area.begin.arith(); current < area.end.arith(); current += PAGE_SIZE)
        {
            const auto *descriptor = lookup(PhyAddr(current));
            if (descriptor == nullptr || descriptor->state != PageState::AVAILABLE ||
                descriptor->kind != PageKind::GENERIC)
                return false;
        }
        for (addr_t current = area.begin.arith(); current < area.end.arith(); current += PAGE_SIZE)
        {
            auto *descriptor     = lookup(PhyAddr(current));
            descriptor->kind     = kind;
            descriptor->state    = PageState::CLAIMED;
            descriptor->owner_id = owner_id;
            descriptor->references.store(1, std::memory_order_relaxed);
        }
        return true;
    }

    void PageDatabase::release(PhyArea area, PageKind kind, u64_t owner_id) noexcept {
        for (addr_t current = area.begin.arith(); current < area.end.arith(); current += PAGE_SIZE)
        {
            auto *descriptor = lookup(PhyAddr(current));
            if (descriptor == nullptr || descriptor->state != PageState::CLAIMED ||
                descriptor->kind != kind || descriptor->owner_id != owner_id)
                kernel::log::panic("无效的 PageDatabase 释放操作");
        }
        for (addr_t current = area.begin.arith(); current < area.end.arith(); current += PAGE_SIZE)
        {
            auto *descriptor = lookup(PhyAddr(current));
            descriptor->references.store(0, std::memory_order_relaxed);
            descriptor->owner_id  = 0;
            descriptor->auxiliary = 0;
            descriptor->flags     = 0;
            ++descriptor->generation;
            descriptor->kind  = PageKind::GENERIC;
            descriptor->state = PageState::AVAILABLE;
        }
    }

    void PageDatabase::release_boot_reclaimable(PhyArea area) noexcept {
        for (addr_t current = area.begin.arith(); current < area.end.arith(); current += PAGE_SIZE)
        {
            auto *descriptor = lookup(PhyAddr(current));
            if (descriptor == nullptr || descriptor->state != PageState::RESERVED ||
                descriptor->kind != PageKind::BOOT_RECLAIMABLE)
                kernel::log::panic("无效的可回收引导 PageDescriptor 状态转换");
        }
        for (addr_t current = area.begin.arith(); current < area.end.arith(); current += PAGE_SIZE)
        {
            auto *descriptor = lookup(PhyAddr(current));
            ++descriptor->generation;
            descriptor->kind  = PageKind::GENERIC;
            descriptor->state = PageState::AVAILABLE;
        }
    }

    const PhysicalMemoryRegion &PageDatabase::region(size_t index) const noexcept {
        if (index >= region_count_)
            kernel::log::panic("PageDatabase 区域索引越界");
        return regions_[index];
    }
}  // namespace memory
