/**
 * @file memory_segment.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief MemorySegment 的懒分配、写入和物理页回收。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/memory_segment.h>
#include <synchronized.h>

#include <cstring>
#include <limits>
#include <new>

namespace memory {
    tay::expected<cap::ObjectRef<MemorySegment>, tay::error_code> MemorySegment::create(
        size_t bytes) noexcept {
        if (bytes == 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        if (bytes > std::numeric_limits<size_t>::max() - (PAGE_SIZE - 1))
            return tay::Err(tay::error_code::ALLOCATION_SIZE_OVERFLOW);
        const size_t aligned = page_align_up(bytes);
        auto pages           = tay::hash_map<size_t, OwnedPages>::try_create(1);
        if (!pages)
            return tay::Err(pages.error());
        auto *object = new (std::nothrow) MemorySegment(aligned, std::move(*pages));
        if (object == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        return cap::ObjectRef<MemorySegment>(*object);
    }

    MemorySegment::~MemorySegment() noexcept = default;

    tay::expected<PhyAddr, tay::error_code> MemorySegment::lookup_page(
        size_t offset) const noexcept {
        if (offset >= size_)
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        kernel::lock_guard<tay::spinlock> guard(lock_);
        const auto iterator = pages_.find(page_align_down(offset) / PAGE_SIZE);
        if (iterator == pages_.end())
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        return iterator->second.base();
    }

    tay::expected<PhyAddr, tay::error_code> MemorySegment::ensure_page(size_t offset) noexcept {
        if (offset >= size_)
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        kernel::lock_guard<tay::spinlock> guard(lock_);
        const size_t index = page_align_down(offset) / PAGE_SIZE;
        if (auto iterator = pages_.find(index); iterator != pages_.end())
            return iterator->second.base();

        auto allocation = gfp(1, PageKind::USER, object_id().value);
        if (!allocation)
            return tay::Err(allocation.error());
        auto *destination = convert<KpaAddr>(allocation->base()).as<std::byte>();
        std::memset(destination, 0, PAGE_SIZE);
        auto inserted = pages_.try_emplace(index, std::move(*allocation));
        if (!inserted)
            return tay::Err(inserted.error());
        return inserted->first->second.base();
    }

    tay::expected<size_t, tay::error_code> MemorySegment::write(size_t offset, const void *data,
                                                                size_t buflen) noexcept {
        if (buflen == 0)
            return size_t{0};
        if (data == nullptr)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        if (offset >= size_)
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        const size_t total = buflen < size_ - offset ? buflen : size_ - offset;
        size_t written     = 0;
        while (written < total) {
            const size_t current = offset + written;
            const size_t chunk   = (PAGE_SIZE - current % PAGE_SIZE) < total - written
                                       ? PAGE_SIZE - current % PAGE_SIZE
                                       : total - written;
            auto page            = ensure_page(current);
            if (!page)
                return tay::Err(page.error());
            auto *destination = convert<KpaAddr>(*page).as<std::byte>() + current % PAGE_SIZE;
            std::memcpy(destination, static_cast<const std::byte *>(data) + written, chunk);
            written += chunk;
        }
        return written;
    }
}  // namespace memory
