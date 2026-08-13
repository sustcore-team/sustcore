/**
 * @file gfp.cpp
 * @brief 原子物理页分配、认领和释放实现。
 */

#include <memory/physical/gfp.h>

#include <utility>

namespace memory {
    OwnedPages::OwnedPages(OwnedPages &&other) noexcept
        : allocation_(other.allocation_), kind_(other.kind_), owner_id_(other.owner_id_) {
        other.allocation_ = {};
        other.kind_       = PageKind::GENERIC;
        other.owner_id_   = 0;
    }

    OwnedPages &OwnedPages::operator=(OwnedPages &&other) noexcept {
        if (this == &other)
            return *this;
        release();
        allocation_       = other.allocation_;
        kind_             = other.kind_;
        owner_id_         = other.owner_id_;
        other.allocation_ = {};
        other.kind_       = PageKind::GENERIC;
        other.owner_id_   = 0;
        return *this;
    }

    OwnedPages::~OwnedPages() noexcept {
        release();
    }

    void OwnedPages::release() noexcept {
        const auto allocation = detach();
        if (!allocation)
            return;
        auto allocator = buddy();
        const PhyArea area(allocation.base, allocation.base + allocation.pages * PAGE_SIZE);
        page_database().release(area, kind_, owner_id_);
        allocator->put_pages(allocation);
    }

    PageAllocation OwnedPages::detach() noexcept {
        const auto allocation = allocation_;
        allocation_           = {};
        return allocation;
    }

    tay::expected<OwnedPages, tay::error_code> gfp(size_t pages, size_t alignment_pages,
                                                   PageKind kind, u64_t owner_id) noexcept {
        if (kind == PageKind::GENERIC)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        auto allocator  = buddy();
        auto allocation = allocator->try_get_free_pages(pages, alignment_pages);
        if (!allocation)
            return tay::Err(allocation.error());
        const PhyArea area(allocation->base, allocation->base + allocation->pages * PAGE_SIZE);
        if (!page_database().claim(area, kind, owner_id)) {
            allocator->put_pages(*allocation);
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        return OwnedPages(*allocation, kind, owner_id);
    }

}  // namespace memory
