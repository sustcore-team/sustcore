/**
 * @file slub.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核固定大小对象缓存管理
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <log.h>
#include <memory/physical/gfp.h>
#include <memory/slab/slub.h>
#include <sustcore/addrspace.h>
#include <tay/bits.h>

#include <atomic>
#include <cstddef>
#include <new>

namespace memory::detail {
    namespace {
        constexpr size_t CHUNK_PAGES      = SLUB_CHUNK_SZ / PAGE_SIZE;
        constexpr u64_t KERNEL_HEAP_OWNER = 0x48454150;
#ifndef NDEBUG
        constexpr size_t ALLOCATION_BITMAP_WORDS = SLUB_CHUNK_SZ / 16 / 64;
#endif

        enum class ChunkState : u8_t {
            EMPTY,
            PARTIAL,
            FULL,
        };

        [[nodiscard]] constexpr addr_t align_up_address(addr_t value, size_t alignment) noexcept {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        [[nodiscard]] constexpr addr_t align_down_address(addr_t value, size_t alignment) noexcept {
            return value & ~(alignment - 1);
        }

        void debug_fill(void *ptr, size_t sz, u8_t value) noexcept {
#ifndef NDEBUG
            auto *bytes = static_cast<u8_t *>(ptr);
            for (size_t i = 0; i < sz; ++i) bytes[i] = value;
#else
            (void)ptr;
            (void)sz;
            (void)value;
#endif
        }
    }  // namespace

    struct SlubChunk {
        AllocationHeader allocation{};
        tay::intrusive_list_hook<SlubChunk *, SlubChunk *> list_hook{};
        ChunkState state = ChunkState::EMPTY;
        void *local_free = nullptr;
        std::atomic<void *> remote_free{nullptr};
        size_t allocated_cnt = 0;
        size_t total_cnt     = 0;
        addr_t first_object  = 0;
#ifndef NDEBUG
        std::atomic<u64_t> allocation_bits[ALLOCATION_BITMAP_WORDS]{};
#endif
    };

    static_assert(offsetof(SlubChunk, allocation) == 0);
    static_assert(sizeof(SlubChunk) < SLUB_CHUNK_SZ);

    tay::intrusive_list_hook<SlubChunk *, SlubChunk *> &ChunkHookLocator::operator()(
        SlubChunk &chunk) const noexcept {
        return chunk.list_hook;
    }

    namespace {
        [[nodiscard]] size_t object_index(const SlubChunk &chunk, const void *object) noexcept {
            const auto addr = reinterpret_cast<addr_t>(object);
            const auto sz   = chunk.allocation.slot_sz;
            if (addr < chunk.first_object || (addr - chunk.first_object) % sz != 0) {
                kernel::log::panic("SLUB 对象未对齐");
            }
            const auto index = (addr - chunk.first_object) / sz;
            if (index >= chunk.total_cnt) {
                kernel::log::panic("SLUB 对象位于块外");
            }
            return index;
        }

        void mark_allocated(SlubChunk &chunk, void *object) noexcept {
#ifndef NDEBUG
            const auto index = object_index(chunk, object);
            const auto mask  = u64_t{1} << (index % 64);
            // bitmap 仅用于原子检测重复分配，不承担对象内容的发布同步。
            const auto previous =
                chunk.allocation_bits[index / 64].fetch_or(mask, std::memory_order_relaxed);
            if ((previous & mask) != 0) {
                kernel::log::panic("SLUB 分配到了正在使用的对象");
            }
#else
            (void)chunk;
            (void)object;
#endif
        }

        void mark_freed(SlubChunk &chunk, void *object) noexcept {
#ifndef NDEBUG
            const auto index = object_index(chunk, object);
            const auto mask  = u64_t{1} << (index % 64);
            const auto previous =
                chunk.allocation_bits[index / 64].fetch_and(~mask, std::memory_order_relaxed);
            if ((previous & mask) == 0) {
                kernel::log::panic("SLUB 对象被重复释放");
            }
#else
            (void)chunk;
            (void)object;
#endif
        }
    }  // namespace

    AllocationHeader *header_for(void *ptr) noexcept {
        if (ptr == nullptr)
            return nullptr;
        const auto addr = reinterpret_cast<addr_t>(ptr);
        if (addr < KPA_START || addr >= KVA_START)
            kernel::log::panic("堆指针不在 HHDM 范围内");
        const PhyAddr pointer_page(page_align_down(KPA2PA(addr)));
        const auto *descriptor = page_database().lookup(pointer_page);
        if (descriptor == nullptr || descriptor->state != PageState::CLAIMED ||
            descriptor->kind != PageKind::KERNEL_HEAP || descriptor->owner_id != KERNEL_HEAP_OWNER)
            kernel::log::panic("堆指针不属于内核堆物理页");
        if ((addr & (SLUB_CHUNK_SZ - 1)) == 0) {
            // 超对齐对象可能恰落在 chunk 边界，需从对象前缀回溯真实的大块头部。
            auto *prefix = reinterpret_cast<OveralignedPrefix *>(addr - sizeof(OveralignedPrefix));
            if (prefix->magic != OVERALIGNED_MAGIC || prefix->header == nullptr) {
                kernel::log::panic("无效的超对齐分配");
            }
            return prefix->header;
        }
        // 普通对象所在 chunk 按固定大小对齐，向下取整即可定位首部 AllocationHeader。
        return reinterpret_cast<AllocationHeader *>(align_down_address(addr, SLUB_CHUNK_SZ));
    }

    tay::expected<void *, tay::error_code> allocate_large(size_t sz, size_t alignment,
                                                          SlubCore *owner) noexcept {
        if (sz == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) {
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        const bool overaligned = alignment >= SLUB_CHUNK_SZ;
        const size_t prefix_sz = overaligned ? sizeof(OveralignedPrefix) : 0;
        if (sz > static_cast<size_t>(-1) - alignment - sizeof(AllocationHeader) - prefix_sz) {
            return tay::Err(tay::error_code::ALLOCATION_SIZE_OVERFLOW);
        }

        const size_t bytes = sizeof(AllocationHeader) + prefix_sz + sz + alignment - 1;
        const size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        auto allocation    = gfp(pages, CHUNK_PAGES, PageKind::KERNEL_HEAP, KERNEL_HEAP_OWNER);
        if (!allocation) {
            return tay::Err(allocation.error());
        }

        auto *header = reinterpret_cast<AllocationHeader *>(PA2KPA(allocation->base().arith()));
        const auto extent = allocation->detach();
        // 分配得到的是原始页，在页首显式构造用于释放和容量查询的元数据对象。
        new (header) AllocationHeader{
            .magic   = ALLOCATION_MAGIC,
            .kind    = AllocationKind::LARGE,
            .owner   = owner,
            .extent  = extent,
            .slot_sz = sz,
        };
        const auto object_address =
            align_up_address(reinterpret_cast<addr_t>(header + 1) + prefix_sz, alignment);
        auto *object = reinterpret_cast<void *>(object_address);
        if (overaligned) {
            // 在返回对象前保存反向指针，解决按 chunk 边界无法定位大块头部的问题。
            auto *prefix =
                reinterpret_cast<OveralignedPrefix *>(object_address - sizeof(OveralignedPrefix));
            new (prefix) OveralignedPrefix{OVERALIGNED_MAGIC, header};
        }
        const auto extent_end = reinterpret_cast<addr_t>(header) + extent.pages * PAGE_SIZE;
        header->capacity      = extent_end - object_address;
        debug_fill(object, sz, 0xA5);
        return object;
    }

    void release_large(AllocationHeader &header, void *ptr) noexcept {
        if (header.magic != ALLOCATION_MAGIC || header.kind != AllocationKind::LARGE) {
            kernel::log::panic("无效的大对象分配");
        }
        const auto allocation = header.extent;
        debug_fill(ptr, header.capacity, 0xDD);
        header.magic = 0;
        OwnedPages::resume(allocation, PageKind::KERNEL_HEAP, KERNEL_HEAP_OWNER).release();
    }

    SlubChunk *SlubCore::create_chunk() noexcept {
        auto allocation = gfp(CHUNK_PAGES, CHUNK_PAGES, PageKind::KERNEL_HEAP, KERNEL_HEAP_OWNER);
        if (!allocation)
            return nullptr;

        auto *chunk       = reinterpret_cast<SlubChunk *>(PA2KPA(allocation->base().arith()));
        const auto extent = allocation->detach();
        new (chunk) SlubChunk{};
        chunk->allocation.owner    = this;
        chunk->allocation.extent   = extent;
        chunk->allocation.slot_sz  = slot_sz_;
        chunk->allocation.capacity = slot_sz_;

        const auto begin    = align_up_address(reinterpret_cast<addr_t>(chunk + 1), slot_align_);
        const auto end      = reinterpret_cast<addr_t>(chunk) + SLUB_CHUNK_SZ;
        chunk->first_object = begin;
        void *head          = nullptr;
        // 空闲对象尚无用户数据，其首个指针大小空间直接用作单链表 next。
        for (auto current = begin; current + slot_sz_ <= end; current += slot_sz_) {
            auto *object                       = reinterpret_cast<void *>(current);
            *reinterpret_cast<void **>(object) = head;
            head                               = object;
            ++chunk->total_cnt;
        }
        if (chunk->total_cnt == 0) {
            kernel::log::panic("SLUB 类无法放入一个块");
        }
        chunk->local_free = head;
        return chunk;
    }

    void SlubCore::MutableState::move_to_partial(SlubChunk &chunk) noexcept {
        if (chunk.state == ChunkState::PARTIAL)
            return;
        if (chunk.state == ChunkState::EMPTY) {
            (void)empty_.remove(&chunk);
        } else {
            (void)full_.remove(&chunk);
        }
        chunk.state = ChunkState::PARTIAL;
        partial_.push_back(&chunk);
    }

    void SlubCore::MutableState::move_to_full(SlubChunk &chunk) noexcept {
        if (chunk.state == ChunkState::FULL)
            return;
        if (chunk.state == ChunkState::EMPTY) {
            (void)empty_.remove(&chunk);
        } else {
            (void)partial_.remove(&chunk);
        }
        chunk.state = ChunkState::FULL;
        full_.push_back(&chunk);
    }

    void SlubCore::MutableState::move_to_empty(SlubChunk &chunk) noexcept {
        if (chunk.state == ChunkState::EMPTY)
            return;
        if (chunk.state == ChunkState::FULL) {
            (void)full_.remove(&chunk);
        } else {
            (void)partial_.remove(&chunk);
        }
        chunk.state = ChunkState::EMPTY;
        empty_.push_back(&chunk);
    }

    void SlubCore::MutableState::drain_remote(SlubChunk &chunk) noexcept {
        // exchange 一次性取得远程链表所有权；并发新释放会进入下一批，不会丢失。
        void *head = chunk.remote_free.exchange(nullptr, std::memory_order_acquire);
        size_t cnt = 0;
        while (head != nullptr) {
            void *next                       = *reinterpret_cast<void **>(head);
            *reinterpret_cast<void **>(head) = chunk.local_free;
            chunk.local_free                 = head;
            ++cnt;
            head = next;
        }
        if (cnt == 0)
            return;
        if (cnt > chunk.allocated_cnt || cnt > objects_in_use_) {
            kernel::log::panic("SLUB 远程释放计数下溢");
        }
        chunk.allocated_cnt -= cnt;
        objects_in_use_     -= cnt;
        if (chunk.allocated_cnt == 0) {
            move_to_empty(chunk);
        } else if (chunk.state == ChunkState::FULL) {
            move_to_partial(chunk);
        }
    }

    SlubChunk *SlubCore::MutableState::select_chunk() noexcept {
        if (!partial_.empty()) {
            auto *chunk = partial_.front();
            drain_remote(*chunk);
            if (chunk->state == ChunkState::EMPTY)
                move_to_partial(*chunk);
            return chunk;
        }
        if (!empty_.empty()) {
            auto *chunk = empty_.front();
            move_to_partial(*chunk);
            return chunk;
        }
        for (auto iterator = full_.begin(); iterator != full_.end(); ++iterator) {
            auto *chunk = *iterator;
            if (chunk->remote_free.load(std::memory_order_acquire) == nullptr) {
                continue;
            }
            drain_remote(*chunk);
            if (chunk->state == ChunkState::EMPTY)
                move_to_partial(*chunk);
            return chunk;
        }
        return nullptr;
    }

    void SlubCore::MutableState::adopt_chunk(SlubChunk &chunk) noexcept {
        if (chunk.state != ChunkState::EMPTY || chunk.allocated_cnt != 0 ||
            chunk.local_free == nullptr ||
            chunk.remote_free.load(std::memory_order_relaxed) != nullptr)
        {
            kernel::log::panic("尝试接管无效的 SLUB 块");
        }
        ++chunks_;
        objects_total_ += chunk.total_cnt;
        empty_.push_back(&chunk);
        move_to_partial(chunk);
    }

    void *SlubCore::MutableState::allocate_from(SlubChunk &chunk) noexcept {
        if (chunk.state != ChunkState::PARTIAL || chunk.local_free == nullptr) {
            kernel::log::panic("SLUB 选中了已满的块");
        }
        void *object     = chunk.local_free;
        chunk.local_free = *reinterpret_cast<void **>(object);
        ++chunk.allocated_cnt;
        ++objects_in_use_;
        mark_allocated(chunk, object);
        if (chunk.local_free == nullptr)
            move_to_full(chunk);
        return object;
    }

    tay::expected<void *, tay::error_code> SlubCore::try_allocate() noexcept {
        if (uses_large_path()) {
            auto result = allocate_large(slot_sz_, slot_align_, this);
            if (!result)
                return result;
            const auto pages = header_for(*result)->extent.pages;
            state_.lock()->account_large_allocation(pages);
            return result;
        }

        void *object = nullptr;
        {
            auto state  = state_.lock();
            auto *chunk = state->select_chunk();
            if (chunk != nullptr) {
                object = state->allocate_from(*chunk);
            }
        }
        if (object == nullptr) {
            auto *candidate = create_chunk();
            if (candidate == nullptr)
                return tay::Err(tay::error_code::OUT_OF_MEMORY);
            auto state  = state_.lock();
            auto *chunk = state->select_chunk();
            state->adopt_chunk(*candidate);
            if (chunk == nullptr)
                chunk = candidate;
            object = state->allocate_from(*chunk);
        }
        debug_fill(object, slot_sz_, 0xA5);
        return object;
    }

    void SlubCore::deallocate_remote(SlubChunk &chunk, void *ptr) noexcept {
        mark_freed(chunk, ptr);
        debug_fill(ptr, slot_sz_, 0xDD);
        void *remote = chunk.remote_free.load(std::memory_order_relaxed);
        // 释放对象自身保存 next，CAS 将其无锁压入 owner 稍后统一接管的远程链表。
        do {
            *reinterpret_cast<void **>(ptr) = remote;
        } while (!chunk.remote_free.compare_exchange_weak(remote, ptr, std::memory_order_release,
                                                          std::memory_order_relaxed));
    }

    PageAllocation SlubCore::MutableState::detach_chunk(SlubChunk &chunk) noexcept {
        if (chunk.allocated_cnt != 0 ||
            chunk.remote_free.load(std::memory_order_relaxed) != nullptr)
        {
            kernel::log::panic("尝试释放忙碌的 SLUB 块");
        }
        if (chunk.state == ChunkState::EMPTY) {
            (void)empty_.remove(&chunk);
        } else if (chunk.state == ChunkState::PARTIAL) {
            (void)partial_.remove(&chunk);
        } else {
            (void)full_.remove(&chunk);
        }
        const auto allocation  = chunk.allocation.extent;
        objects_total_        -= chunk.total_cnt;
        --chunks_;
        chunk.allocation.magic = 0;
        return allocation;
    }

    PageAllocation SlubCore::MutableState::detach_excess_empty(SlubChunk &chunk) noexcept {
        if (chunk.state != ChunkState::EMPTY || empty_.size() <= 1)
            return {};
        return detach_chunk(chunk);
    }

    PageAllocation SlubCore::MutableState::detach_empty() noexcept {
        if (empty_.empty())
            return {};
        return detach_chunk(*empty_.front());
    }

    void SlubCore::MutableState::drain_remote_frees() noexcept {
        for (auto iterator = full_.begin(); iterator != full_.end();) {
            auto current = iterator++;
            drain_remote(**current);
        }
        for (auto iterator = partial_.begin(); iterator != partial_.end();) {
            auto current = iterator++;
            drain_remote(**current);
        }
    }

    void SlubCore::MutableState::deallocate_to(SlubChunk &chunk, void *ptr) noexcept {
        mark_freed(chunk, ptr);
        debug_fill(ptr, chunk.allocation.slot_sz, 0xDD);
        *reinterpret_cast<void **>(ptr) = chunk.local_free;
        chunk.local_free                = ptr;
        if (chunk.allocated_cnt == 0 || objects_in_use_ == 0) {
            kernel::log::panic("SLUB 分配计数下溢");
        }
        --chunk.allocated_cnt;
        --objects_in_use_;
        if (chunk.allocated_cnt == 0) {
            move_to_empty(chunk);
        } else if (chunk.state == ChunkState::FULL) {
            move_to_partial(chunk);
        }
    }

    void SlubCore::MutableState::account_large_allocation(size_t pages) noexcept {
        ++chunks_;
        ++objects_in_use_;
        ++objects_total_;
        large_pages_ += pages;
    }

    void SlubCore::MutableState::account_large_release(size_t pages) noexcept {
        if (chunks_ == 0 || objects_in_use_ == 0 || objects_total_ == 0 || large_pages_ < pages) {
            kernel::log::panic("大对象 SLUB 记账下溢");
        }
        --chunks_;
        --objects_in_use_;
        --objects_total_;
        large_pages_ -= pages;
    }

    SlubStats SlubCore::MutableState::stats(bool large_path) const noexcept {
        return SlubStats{
            .chunks         = chunks_,
            .objects_in_use = objects_in_use_,
            .objects_total  = objects_total_,
            .resident_pages = large_path ? large_pages_ : chunks_ * CHUNK_PAGES,
        };
    }

    void SlubCore::deallocate(void *ptr) noexcept {
        if (ptr == nullptr)
            return;
        auto *header = header_for(ptr);
        if (header->magic != ALLOCATION_MAGIC || header->slot_sz != slot_sz_) {
            kernel::log::panic("分配经由错误的 SLUB 被释放");
        }
        if (header->kind == AllocationKind::LARGE) {
            auto *owner      = header->owner;
            const auto pages = header->extent.pages;
            if (owner != nullptr) {
                owner->state_.lock()->account_large_release(pages);
            }
            release_large(*header, ptr);
            return;
        }
        if (header->kind != AllocationKind::SLAB || header->owner == nullptr) {
            kernel::log::panic("无效的 SLUB 分配头");
        }

        auto &chunk = *reinterpret_cast<SlubChunk *>(header);
        if (header->owner != this) {
            // chunk 状态只由 owner 的锁保护，跨 owner 释放只能先进入原子远程链表。
            header->owner->deallocate_remote(chunk, ptr);
            return;
        }

        PageAllocation allocation{};
        {
            auto state = state_.lock();
            state->deallocate_to(chunk, ptr);
            allocation = state->detach_excess_empty(chunk);
        }
        if (allocation) {
            OwnedPages::resume(allocation, PageKind::KERNEL_HEAP, KERNEL_HEAP_OWNER).release();
        }
    }

    void SlubCore::trim() noexcept {
        if (uses_large_path())
            return;
        {
            auto state = state_.lock();
            state->drain_remote_frees();
        }
        while (true) {
            PageAllocation allocation{};
            {
                auto state = state_.lock();
                allocation = state->detach_empty();
            }
            if (!allocation)
                break;
            OwnedPages::resume(allocation, PageKind::KERNEL_HEAP, KERNEL_HEAP_OWNER).release();
        }
    }

    SlubStats SlubCore::stats() const noexcept {
        return state_.lock()->stats(uses_large_path());
    }
}  // namespace memory::detail
