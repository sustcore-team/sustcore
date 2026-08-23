/**
 * @file mem_seg.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief MemSeg 的懒分配、写入和物理页回收。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/mem_seg.h>

#include <cstring>
#include <limits>
#include <new>

namespace memory {
    namespace {
        struct PageSlice final {
            size_t index     = 0;
            size_t in_page   = 0;
            size_t available = 0;
        };

        [[nodiscard]] tay::expected<PageSlice, MemSegError> page_slice(
            size_t offset, size_t segment_size) noexcept {
            if (offset >= segment_size)
                return tay::Err(MemSegError::OffsetOutOfRange(offset, segment_size));
            const size_t in_page         = offset % PAGE_SIZE;
            const size_t in_segment      = segment_size - offset;
            const size_t in_current_page = PAGE_SIZE - in_page;
            return PageSlice{
                .index     = offset / PAGE_SIZE,
                .in_page   = in_page,
                .available = in_segment < in_current_page ? in_segment : in_current_page,
            };
        }
    }  // namespace

    tay::expected<cap::KObjectRef<MemSeg>, MemSegError> MemSeg::create(size_t bytes) noexcept {
        if (bytes == 0)
            return tay::Err(MemSegError::ZeroSize());
        if (bytes > std::numeric_limits<size_t>::max() - (PAGE_SIZE - 1))
            return tay::Err(MemSegError::SizeOverflow(bytes));
        const size_t aligned = page_align_up(bytes);
        auto pages           = tay::hash_map<size_t, OwnedPages>::try_create(1);
        if (!pages)
            return tay::Err(MemSegError::OutOfMemory());
        auto *object = new (std::nothrow) MemSeg(aligned, std::move(*pages));
        if (object == nullptr)
            return tay::Err(MemSegError::OutOfMemory());
        return cap::KObjectRef<MemSeg>(*object);
    }

    MemSeg::~MemSeg() noexcept = default;

    size_t MemSeg::allocated_size() const noexcept {
        auto state = state_.lock();
        return state->pages.size() * PAGE_SIZE;
    }

    tay::expected<PhyAddr, MemSegError> MemSeg::lookup_page(size_t offset) const noexcept {
        const auto slice    = TAY_TRY(page_slice(offset, size_));
        auto state          = state_.lock();
        const auto iterator = state->pages.find(slice.index);
        if (iterator == state->pages.end())
            return tay::Err(MemSegError::PageNotAllocated(slice.index));
        return iterator->second.base();
    }

    tay::expected<PhyAddr, MemSegError> MemSeg::ensure_page(size_t offset) noexcept {
        const auto slice = TAY_TRY(page_slice(offset, size_));
        auto state       = state_.lock();
        if (auto iterator = state->pages.find(slice.index); iterator != state->pages.end())
            return iterator->second.base();

        auto allocation = gfp(1, PageKind::USER, object_id().value);
        if (!allocation)
            return tay::Err(MemSegError::PhysAllocFailed(
                slice.index, kernel::from_tay_error(allocation.error())
                                 .value_or(kernel::KernelError::TayError::INTERNAL)));
        auto *destination = convert<KpaAddr>(allocation->base()).as<std::byte>();
        std::memset(destination, 0, PAGE_SIZE);
        auto inserted = state->pages.try_emplace(slice.index, std::move(*allocation));
        if (!inserted)
            return tay::Err(MemSegError::PageInsertFailed(
                slice.index, kernel::from_tay_error(inserted.error())
                                 .value_or(kernel::KernelError::TayError::INTERNAL)));
        return inserted->first->second.base();
    }

    tay::expected<size_t, MemSegError> MemSeg::write(size_t offset, const void *data,
                                                     size_t buflen) noexcept {
        if (buflen == 0)
            return size_t{0};
        if (data == nullptr)
            return tay::Err(MemSegError::InvalidSourceBuffer());
        static_cast<void>(TAY_TRY(page_slice(offset, size_)));
        const size_t total = buflen < size_ - offset ? buflen : size_ - offset;
        size_t written     = 0;
        while (written < total) {
            const size_t current = offset + written;
            const auto slice     = TAY_TRY(page_slice(current, size_));
            const size_t chunk =
                slice.available < total - written ? slice.available : total - written;
            const PhyAddr page = TAY_TRY(ensure_page(current));
            auto *destination  = convert<KpaAddr>(page).as<std::byte>() + slice.in_page;
            std::memcpy(destination, static_cast<const std::byte *>(data) + written, chunk);
            written += chunk;
        }
        return written;
    }
}  // namespace memory
