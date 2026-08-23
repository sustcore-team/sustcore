/**
 * @file heap.cpp
 * @brief 永久全局 MixedSlabsAllocator 及单向发布状态。
 */

#include <log.h>
#include <memory/physical/page_db.h>
#include <memory/slab/heap.h>

#include <atomic>
#include <cstddef>

namespace memory {
    namespace {
        constinit std::atomic<HeapPhase> phase{HeapPhase::OFFLINE};
        constinit MixedSlabsAllocator global_allocator;
    }  // namespace

    tay::expected<void *, tay::error_code> MixedSlabsAllocator::try_allocate(
        size_t sz, size_t alignment) noexcept {
        if (alignment == 0 || (alignment & (alignment - 1)) != 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        if (sz == 0)
            sz = 1;
        auto *pool = slubs_.find(sz, alignment);
        if (pool == nullptr)
            return detail::allocate_large(sz, alignment);
        return pool->try_allocate();
    }

    void MixedSlabsAllocator::deallocate(void *ptr) noexcept {
        if (ptr == nullptr)
            return;
        auto *header = detail::header_for(ptr);
        if (header->magic != detail::ALLOCATION_MAGIC)
            kernel::log::panic("无效的堆指针");
        if (header->kind == detail::AllocationKind::LARGE) {
            detail::release_large(*header, ptr);
            return;
        }
        if (header->kind != detail::AllocationKind::SLAB)
            kernel::log::panic("无效的堆分配类别");
        auto *pool = slubs_.find_exact(header->slot_sz);
        if (pool == nullptr)
            kernel::log::panic("无效的堆 SLUB 类");
        pool->deallocate(ptr);
    }

    size_t MixedSlabsAllocator::usable_sz(void *ptr) const noexcept {
        if (ptr == nullptr)
            return 0;
        const auto *header = detail::header_for(ptr);
        if (header->magic != detail::ALLOCATION_MAGIC)
            kernel::log::panic("无效的堆指针");
        return header->kind == detail::AllocationKind::LARGE ? header->capacity : header->slot_sz;
    }

    void *MixedSlabsAllocator::reallocate(void *ptr, size_t new_sz) noexcept {
        if (ptr == nullptr) {
            auto result = try_allocate(new_sz);
            return result ? *result : nullptr;
        }
        if (new_sz == 0) {
            deallocate(ptr);
            return nullptr;
        }
        const size_t old_sz = usable_sz(ptr);
        if (new_sz <= old_sz)
            return ptr;
        auto replacement = try_allocate(new_sz);
        if (!replacement)
            return nullptr;
        auto *output      = static_cast<u8_t *>(*replacement);
        const auto *input = static_cast<const u8_t *>(ptr);
        for (size_t i = 0; i < old_sz; ++i) output[i] = input[i];
        deallocate(ptr);
        return *replacement;
    }

    void MixedSlabsAllocator::trim() noexcept {
        for_each_slub([](auto &pool) noexcept { pool.trim(); });
    }

    tay::expected<void, tay::error_code> init_heap() noexcept {
        if (page_db().region_count() == 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        HeapPhase expected = HeapPhase::OFFLINE;
        if (!phase.compare_exchange_strong(expected, HeapPhase::INITIALIZING,
                                           std::memory_order_acq_rel, std::memory_order_acquire))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        auto small = global_allocator.try_allocate(256, alignof(std::max_align_t));
        if (!small) {
            global_allocator.trim();
            phase.store(HeapPhase::OFFLINE, std::memory_order_release);
            return TAY_ERR(small);
        }
        global_allocator.deallocate(*small);

        auto large = global_allocator.try_allocate(64 * 1024, PAGE_SIZE);
        if (!large) {
            global_allocator.trim();
            phase.store(HeapPhase::OFFLINE, std::memory_order_release);
            return TAY_ERR(large);
        }
        global_allocator.deallocate(*large);
        phase.store(HeapPhase::READY, std::memory_order_release);
        return {};
    }

    bool heap_ready() noexcept {
        return phase.load(std::memory_order_acquire) == HeapPhase::READY;
    }

    MixedSlabsAllocator *try_heap_allocator() noexcept {
        return heap_ready() ? &global_allocator : nullptr;
    }

    MixedSlabsAllocator &heap_allocator() noexcept {
        auto *allocator = try_heap_allocator();
        if (allocator == nullptr)
            kernel::log::panic("内核堆尚未就绪");
        return *allocator;
    }
}  // namespace memory
