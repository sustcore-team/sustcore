/**
 * @file memory_segment.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 匿名、固定大小且按需分配物理页的 MemorySegment 对象。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <memory/memory_segment_error.h>
#include <memory/physical/gfp.h>
#include <obj/kernel_object.h>
#include <sustcore/addr.h>
#include <tay/expected.h>
#include <tay/map.h>
#include <tay/spinlock.h>

#include <cstddef>
#include <utility>

namespace memory {
    class MemorySegment final
        : public cap::TypedKernelObject<MemorySegment, cap::ObjectType::MEMORY> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::MEMORY;

        [[nodiscard]] static tay::expected<cap::ObjectRef<MemorySegment>, MemorySegmentError>
        create(size_t bytes) noexcept;

        MemorySegment(const MemorySegment &)            = delete;
        MemorySegment &operator=(const MemorySegment &) = delete;
        MemorySegment(MemorySegment &&)                 = delete;
        MemorySegment &operator=(MemorySegment &&)      = delete;
        ~MemorySegment() noexcept;

        [[nodiscard]] size_t size() const noexcept {
            return size_;
        }

        [[nodiscard]] size_t allocated_size() const noexcept {
            return pages_.size() * PAGE_SIZE;
        }

        [[nodiscard]] tay::expected<PhyAddr, MemorySegmentError> ensure_page(
            size_t offset) noexcept;
        [[nodiscard]] tay::expected<PhyAddr, MemorySegmentError> lookup_page(
            size_t offset) const noexcept;
        [[nodiscard]] tay::expected<size_t, MemorySegmentError> write(size_t offset,
                                                                      const void *data,
                                                                      size_t buflen) noexcept;

    private:
        explicit MemorySegment(size_t bytes, tay::hash_map<size_t, OwnedPages> &&pages) noexcept
            : size_(bytes), pages_(std::move(pages)) {}

        size_t size_ = 0;
        mutable tay::spinlock lock_{};
        tay::hash_map<size_t, OwnedPages> pages_{};
    };
}  // namespace memory
