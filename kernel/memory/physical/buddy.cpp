/**
 * @file buddy.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核物理页分配与资源管理
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/interrupt.h>
#include <exec_ctx.h>
#include <log.h>
#include <memory/physical/buddy.h>
#include <memory/physical/page_db.h>
#include <sustcore/addrspace.h>
#include <tay/bits.h>

#include <cstddef>
#include <new>

namespace memory {
    namespace {
        constinit kernel::synchronized<Buddy> global_buddy;
        constexpr u64_t BUDDY_METADATA_OWNER = 0x42554444594D4554ULL;
    }  // namespace

    kernel::locked_ref<Buddy> buddy() noexcept {
        return global_buddy.lock();
    }

    size_t Buddy::ceil_order(size_t pages) noexcept {
        size_t order = 0;
        size_t sz    = 1;
        while (sz < pages && order < MAX_ORDER) {
            sz <<= 1;
            ++order;
        }
        return order;
    }

    void Buddy::init_pool(DescPool &pool, PageAlloc backing) noexcept {
        pool.previous = nullptr;
        pool.next     = nullptr;
        pool.backing  = backing;
        pool.used     = 0;
        for (auto &word : pool.bitmap) word = 0;
        for (size_t i = 0; i < DESCS_PER_POOL; ++i) {
            auto &block    = pool.blocks[i];
            block.previous = nullptr;
            block.next     = nullptr;
            block.pool     = &pool;
            block.physical = 0;
            block.order    = 0;
            block.slot     = static_cast<u16_t>(i);
            block.magic    = 0;
        }
    }

    void Buddy::attach_pool(DescPool &pool) noexcept {
        pool.previous = pool_tail_;
        pool.next     = nullptr;
        if (pool_tail_ != nullptr) {
            pool_tail_->next = &pool;
        } else {
            pool_head_ = &pool;
        }
        pool_tail_ = &pool;
    }

    size_t Buddy::free_descs() const noexcept {
        size_t available = 0;
        for (auto *pool = pool_head_; pool != nullptr; pool = pool->next) {
            available += DESCS_PER_POOL - pool->used;
        }
        return available;
    }

    Buddy::FreeBlock *Buddy::allocate_from_pool(DescPool &pool) noexcept {
        if (pool.used == DESCS_PER_POOL)
            return nullptr;
        for (size_t word_index = 0; word_index < DESC_BITMAP_WORDS; ++word_index) {
            auto word = pool.bitmap[word_index];
            if (word == static_cast<u64_t>(-1))
                continue;
            for (size_t bit = 0; bit < 64; ++bit) {
                const auto mask = u64_t{1} << bit;
                if ((word & mask) != 0)
                    continue;
                const size_t slot        = word_index * 64 + bit;
                pool.bitmap[word_index] |= mask;
                ++pool.used;
                auto &block    = pool.blocks[slot];
                block.previous = nullptr;
                block.next     = nullptr;
                block.physical = 0;
                block.order    = 0;
                block.magic    = DESCRIPTOR_MAGIC;
                return &block;
            }
        }
        return nullptr;
    }

    Buddy::FreeBlock *Buddy::alloc_desc() noexcept {
        // 预留足够 descriptor，使一次高阶块拆分不会在扩容途中耗尽元数据。
        if (!expanding_pool_ && free_descs() <= DESCRIPTOR_RESERVE) {
            ensure_descs();
        }
        for (auto *pool = pool_head_; pool != nullptr; pool = pool->next) {
            if (auto *block = allocate_from_pool(*pool); block != nullptr) {
                return block;
            }
        }
        kernel::log::panic("Buddy 描述符池已耗尽");
    }

    void Buddy::free_desc(FreeBlock &block) noexcept {
        if (block.magic != DESCRIPTOR_MAGIC || block.pool == nullptr ||
            block.slot >= DESCS_PER_POOL)
        {
            kernel::log::panic("无效的 Buddy 描述符");
        }
        auto &pool              = *block.pool;
        const size_t word_index = block.slot / 64;
        const auto mask         = u64_t{1} << (block.slot % 64);
        if ((pool.bitmap[word_index] & mask) == 0 || pool.used == 0) {
            kernel::log::panic("Buddy 描述符被重复释放");
        }
        pool.bitmap[word_index] &= ~mask;
        --pool.used;
        block.previous = nullptr;
        block.next     = nullptr;
        block.physical = 0;
        block.order    = 0;
        block.magic    = 0;
    }

    void Buddy::ensure_descs() noexcept {
        if (expanding_pool_ || free_descs() > DESCRIPTOR_RESERVE) {
            return;
        }
        add_runtime_pool();
    }

    void Buddy::add_runtime_pool() noexcept {
        if (expanding_pool_) {
            kernel::log::panic("Buddy 描述符扩容发生递归");
        }
        expanding_pool_ = true;
        // descriptor pool 自身也来自 Buddy；标志位阻止该分配递归触发再次扩容。
        auto allocation = allocate_block(ceil_order(DESC_POOL_PAGES));
        expanding_pool_ = false;
        if (!allocation) {
            kernel::log::panic("无法分配 Buddy 描述符池");
        }

        PageAlloc backing{.base = PhyAddr(*allocation), .pages = DESC_POOL_PAGES};
        const PhyArea backing_area(backing.base, backing.base + backing.pages * PAGE_SIZE);
        if (!page_db().claim(backing_area, PageKind::METADATA, BUDDY_METADATA_OWNER))
            kernel::log::panic("无法认领 Buddy 描述符池物理页");
        // PA2KPA 提供可访问别名，placement new 在原始物理页上建立 DescPool 对象生命周期。
        auto *pool = reinterpret_cast<DescPool *>(PA2KPA(*allocation));
        new (pool) DescPool{};
        init_pool(*pool, backing);
        attach_pool(*pool);
        ++runtime_pool_count_;
    }

    Buddy::FreeBlock *Buddy::find(size_t order, addr_t physical) noexcept {
        for (auto *node = free_[order]; node != nullptr; node = node->next) {
            if (node->physical == physical)
                return node;
            if (node->physical > physical)
                break;
        }
        return nullptr;
    }

    Buddy::FreeBlock *Buddy::insert(addr_t physical, size_t order) noexcept {
        auto **cursor       = &free_[order];
        FreeBlock *previous = nullptr;
        while (*cursor != nullptr && (*cursor)->physical < physical) {
            previous = *cursor;
            cursor   = &(*cursor)->next;
        }
        if (*cursor != nullptr && (*cursor)->physical == physical) {
            kernel::log::panic("Buddy 在 {} 处发生重复释放", physical);
        }

        auto *node     = alloc_desc();
        node->physical = physical;
        node->order    = static_cast<u32_t>(order);
        node->previous = previous;
        node->next     = *cursor;
        if (*cursor != nullptr)
            (*cursor)->previous = node;
        *cursor      = node;
        free_pages_ += size_t{1} << order;
        return node;
    }

    void Buddy::remove(FreeBlock &block) noexcept {
        if (block.magic != DESCRIPTOR_MAGIC || block.order > MAX_ORDER) {
            kernel::log::panic("Buddy 空闲块已损坏");
        }
        if (block.previous != nullptr) {
            block.previous->next = block.next;
        } else {
            free_[block.order] = block.next;
        }
        if (block.next != nullptr)
            block.next->previous = block.previous;
        free_pages_ -= size_t{1} << block.order;
        free_desc(block);
    }

    void Buddy::release_block(addr_t physical, size_t order) noexcept {
        // 异或当前块大小可得到同阶伙伴；只有伙伴空闲时才能继续向上合并。
        while (order < MAX_ORDER) {
            const addr_t bytes = (size_t{1} << order) * PAGE_SIZE;
            const addr_t buddy = physical ^ bytes;
            auto *other        = find(order, buddy);
            if (other == nullptr)
                break;
            remove(*other);
            if (buddy < physical)
                physical = buddy;
            ++order;
        }
        (void)insert(physical, order);
    }

    void Buddy::release_range(addr_t physical, size_t pages) noexcept {
        while (pages != 0) {
            size_t order = 0;
            while (order < MAX_ORDER) {
                const size_t next_pages = size_t{1} << (order + 1);
                const addr_t next_bytes = next_pages * PAGE_SIZE;
                if (next_pages > pages || (physical & (next_bytes - 1)) != 0) {
                    break;
                }
                ++order;
            }
            release_block(physical, order);
            const size_t block_pages  = size_t{1} << order;
            physical                 += block_pages * PAGE_SIZE;
            pages                    -= block_pages;
        }
    }

    tay::expected<addr_t, tay::error_code> Buddy::allocate_block(size_t order) noexcept {
        if (order > MAX_ORDER) {
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        size_t found = order;
        while (found <= MAX_ORDER && free_[found] == nullptr) ++found;
        if (found > MAX_ORDER) {
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        }

        auto *block           = free_[found];
        const addr_t physical = block->physical;
        remove(*block);
        // 逐级拆分高阶块，保留低地址半块并把另一半重新挂回对应 free list。
        while (found > order) {
            --found;
            const addr_t sibling = physical + (size_t{1} << found) * PAGE_SIZE;
            (void)insert(sibling, found);
        }
        return physical;
    }

    void Buddy::initialize() noexcept {
        if (initialized_)
            kernel::log::panic("Buddy 永久描述符池被重复初始化");

        init_pool(permanent_pool_, {});
        attach_pool(permanent_pool_);
        initialized_ = true;
    }

    void Buddy::add_range(PhyArea area) noexcept {
        kernel::assert_task_ctx();
        if (!initialized_) {
            kernel::log::panic("Buddy 永久描述符池初始化前就添加了区域");
        }
        if (area.begin.arith() >= area.end.arith() || !area.begin.aligned<PAGE_SIZE>() ||
            !area.end.aligned<PAGE_SIZE>())
        {
            kernel::log::panic("无效的 Buddy 区域");
        }
        release_range(area.begin.arith(), (area.end.arith() - area.begin.arith()) / PAGE_SIZE);
    }

    tay::expected<PageAlloc, tay::error_code> Buddy::try_alloc_order(size_t order) noexcept {
        kernel::assert_task_ctx();
        ensure_descs();
        const addr_t result = TAY_TRY(allocate_block(order));
        return PageAlloc{.base = PhyAddr(result), .pages = size_t{1} << order};
    }

    tay::expected<PageAlloc, tay::error_code> Buddy::try_alloc_pages(
        size_t pages, size_t alignment_pages) noexcept {
        kernel::assert_task_ctx();
        if (pages == 0 || alignment_pages == 0 || (alignment_pages & (alignment_pages - 1)) != 0) {
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        const size_t allocation_order =
            ceil_order(pages > alignment_pages ? pages : alignment_pages);
        if (allocation_order > MAX_ORDER) {
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        }

        ensure_descs();
        const addr_t result          = TAY_TRY(allocate_block(allocation_order));
        const size_t allocated_pages = size_t{1} << allocation_order;
        if (allocated_pages > pages) {
            // Buddy 只能按二次幂取块，多出的尾部立即拆解归还。
            release_range(result + pages * PAGE_SIZE, allocated_pages - pages);
        }
        return PageAlloc{.base = PhyAddr(result), .pages = pages};
    }

    void Buddy::free_pages(PageAlloc allocation) noexcept {
        kernel::assert_task_ctx();
        if (!allocation)
            return;
        ensure_descs();
        release_range(allocation.base.arith(), allocation.pages);
    }

}  // namespace memory
