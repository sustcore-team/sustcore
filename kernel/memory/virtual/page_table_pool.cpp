/**
 * @file page_table_pool.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页表页分配、声明与本地退役
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <memory/physical/buddy.h>
#include <memory/physical/page_database.h>
#include <memory/virtual/page_table_pool.h>
#include <sustcore/addrspace.h>

#include <cstddef>
#include <cstring>

namespace memory::paging {
    namespace {
        constexpr u32_t PAGE_TABLE_RETIREMENT_FLAG = u32_t{1} << 0;

        [[nodiscard]] PageDescriptor &retirement_descriptor(PhyAddr page,
                                                            PageTableOwnerId owner) noexcept {
            if (!page.nonnull() || !page.aligned<PAGE_SIZE>())
                kernel::log::panic("无效的页表回收操作");
            auto *descriptor = page_database().lookup(page);
            if (descriptor == nullptr || descriptor->state != PageState::CLAIMED ||
                descriptor->kind != PageKind::PAGE_TABLE || descriptor->owner_id != owner)
                kernel::log::panic("无效的页表回收操作");
            return *descriptor;
        }
    }  // namespace

    tay::expected<PhyAddr, tay::error_code> PageAllocator::allocate(
        PageTableOwnerId owner) noexcept {
        auto allocation = buddy()->try_get_free_pages(1);
        if (!allocation)
            return tay::Err(allocation.error());
        const PhyArea area(allocation->base, allocation->base + PAGE_SIZE);
        if (!page_database().claim(area, PageKind::PAGE_TABLE, owner)) {
            buddy()->put_pages(*allocation);
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        memset(reinterpret_cast<void *>(PA2KPA(allocation->base.arith())), 0, PAGE_SIZE);
        return allocation->base;
    }

    void PageAllocator::retire(PhyAddr page, PageTableOwnerId owner) noexcept {
        if (!page.nonnull() || !page.aligned<PAGE_SIZE>())
            return;
        const PhyArea area(page, page + PAGE_SIZE);
        page_database().release(area, PageKind::PAGE_TABLE, owner);
        buddy()->put_pages(PageAllocation{page, 1});
    }

    void RetirementSink::defer_table(PhyAddr page) noexcept {
        auto &descriptor = retirement_descriptor(page, owner_);
        if ((descriptor.flags & PAGE_TABLE_RETIREMENT_FLAG) != 0)
            kernel::log::panic("页表页被重复回收");
        descriptor.auxiliary  = head_.arith();
        descriptor.flags     |= PAGE_TABLE_RETIREMENT_FLAG;
        head_                 = page;
    }

    void RetirementSink::retire_all() noexcept {
        while (head_.nonnull()) {
            const PhyAddr page = head_;
            auto &descriptor   = retirement_descriptor(page, owner_);
            if ((descriptor.flags & PAGE_TABLE_RETIREMENT_FLAG) == 0)
                kernel::log::panic("无效的页表回收队列");
            head_                 = PhyAddr(descriptor.auxiliary);
            descriptor.auxiliary  = 0;
            descriptor.flags     &= ~PAGE_TABLE_RETIREMENT_FLAG;
            PageAllocator::retire(page, owner_);
        }
    }
}  // namespace memory::paging
