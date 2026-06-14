/**
 * @file buddy.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Buddy页框分配器
 * @version alpha-1.0.0
 * @date 2025-11-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <arch/trait.h>
#include <logger.h>
#include <mem/gfp_def.h>
#include <sustcore/addr.h>

#include <cstddef>

class BuddyAllocator {
public:
    struct FreeBlock {
        PhyAddr paddr;
        size_t order;
        FreeBlock *prev;
        FreeBlock *next;
    };

    static constexpr int MAX_BUDDY_ORDER           = 15;
    static constexpr size_t FREEBLOCK_POOL_SIZE    = 512;
    static constexpr size_t FREEBLOCK_EXPAND_PAGES = 4;

    static void pre_init();
    static void post_init();

    template <KernelStage Stage = KernelStage::POST_INIT>
    static Result<PhyAddr> get_free_page(size_t frame_count);

    template <KernelStage Stage = KernelStage::POST_INIT>
    static Result<PhyAddr> get_free_pages_in_order(size_t order);

    template <KernelStage Stage = KernelStage::POST_INIT>
    static void put_page(PhyAddr paddr, size_t frame_count);

    template <KernelStage Stage = KernelStage::POST_INIT>
    static void put_page_in_order(PhyAddr paddr, int order);

    template <KernelStage Stage = KernelStage::POST_INIT>
    static void __print_memory_layout() {
        loggers::BUDDY::DEBUG("Buddy Allocator Memory Layout (Stage: %d):",
                              static_cast<int>(Stage));
        for (int order = 0; order <= MAX_BUDDY_ORDER; ++order) {
            size_t count = 0;
            for (FreeBlock *node = free_area[order]; node != nullptr;
                 node             = node->next)
            {
                ++count;
            }

            loggers::BUDDY::DEBUG("Order %d: %u blocks", order,
                                  static_cast<unsigned>(count));
            for (FreeBlock *node = free_area[order]; node != nullptr;
                 node             = node->next)
            {
                loggers::BUDDY::DEBUG("    Free block at [%p, %p)",
                                      node->paddr.addr(),
                                      (node->paddr +
                                       (1ul << (order + 12))).addr());
            }
        }
    }

private:
    struct FreeBlockPool {
        size_t bitmap[8];
        size_t used;
        FreeBlockPool *next;
        FreeBlockPool *prev;
    };

    static_assert(sizeof(FreeBlockPool) <= sizeof(FreeBlock) * 4,
                  "FreeBlockPool 头必须位于池头四个块内");

    inline static FreeBlock _buddy_pool0[FREEBLOCK_POOL_SIZE];
    inline static FreeBlockPool *_pool_head = nullptr;
    inline static FreeBlockPool *_pool_tail = nullptr;
    inline static FreeBlock *free_area[MAX_BUDDY_ORDER + 1] = {};
    inline static bool _post_initialized                    = false;

    static constexpr size_t FREEBLOCK_HEADER_BLOCKS = 4;
    static constexpr size_t FREEBLOCK_HEADER_BYTES  =
        sizeof(FreeBlock) * FREEBLOCK_HEADER_BLOCKS;

    template <KernelStage Stage>
    static void add_memory_range(PhyAddr paddr, size_t pages);

    static constexpr int pages2order(size_t count) {
        switch (count) {
            case 1:  return 0;
            case 2:  return 1;
            case 3:
            case 4:  return 2;
            default: {
                size_t order = 3;
                while (order <= MAX_BUDDY_ORDER) {
                    if ((1ul << order) >= count) {
                        break;
                    }
                    ++order;
                }
                return static_cast<int>(order);
            }
        }
    }

    static constexpr size_t block_size_for_order(size_t order) {
        return 1ul << (order + 12);
    }

    static constexpr size_t block_index(size_t used) {
        return used - FREEBLOCK_HEADER_BLOCKS;
    }

    static constexpr size_t bit_index(size_t idx) {
        return idx / (sizeof(size_t) * 8);
    }

    static constexpr size_t bit_mask(size_t idx) {
        return 1ul << (idx % (sizeof(size_t) * 8));
    }

    static PhyArea pool0_area() noexcept;
    static FreeBlockPool *pool0_head() noexcept;
    template <KernelStage Stage>
    static FreeBlockPool *runtime_pool(PhyAddr paddr) noexcept;

    template <KernelStage Stage>
    static FreeBlock *runtime_block(PhyAddr paddr) noexcept;

    static void init_pool0() noexcept;
    template <KernelStage Stage>
    static void init_pool(FreeBlockPool *pool, PhyAddr pool_paddr) noexcept;
    static PhyAddr pool_to_pa(const FreeBlockPool *pool) noexcept;
    static FreeBlock *pool_to_blocks(const FreeBlockPool *pool) noexcept;
    static void attach_pool(FreeBlockPool *pool) noexcept;
    static bool pool_needs_expand(const FreeBlockPool *pool) noexcept;
    template <KernelStage Stage>
    static Result<void> maybe_expand_pool() noexcept;
    template <KernelStage Stage>
    static Result<void> add_new_pool() noexcept;

    template <KernelStage Stage>
    static Result<FreeBlock *> alloc_freeblock() noexcept;
    static void free_freeblock(FreeBlock *block) noexcept;

    static void link_block(FreeBlock *node) noexcept;
    static void unlink_block(FreeBlock *node) noexcept;
    static FreeBlock *find_buddy_node(FreeBlock *node) noexcept;

    template <KernelStage Stage>
    static Result<PhyAddr> fetch_frame_order(size_t order);
};

template <KernelStage Stage>
void BuddyAllocator::add_memory_range(PhyAddr paddr, size_t pages) {
    size_t remain = pages;
    PhyAddr addr  = paddr;

    while (remain > 0) {
        size_t order = 0;
        while (order < MAX_BUDDY_ORDER) {
            size_t try_pages = 1UL << (order + 1);
            size_t try_size  = try_pages << 12;
            if (try_pages <= remain && addr.aligned(try_size)) {
                ++order;
            } else {
                break;
            }
        }

        put_page_in_order<Stage>(addr, static_cast<int>(order));

        size_t block_pages = 1ul << order;
        addr              += block_pages << 12;
        remain            -= block_pages;
    }
}

template <KernelStage Stage>
Result<PhyAddr> BuddyAllocator::get_free_page(size_t frame_count) {
    if (frame_count == 0) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    if (frame_count > (1ul << MAX_BUDDY_ORDER)) {
        loggers::BUDDY::ERROR("请求的页数 %u 超出最大支持的范围",
                              static_cast<unsigned>(frame_count));
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto fetch_res = fetch_frame_order<Stage>(pages2order(frame_count));
    if (!fetch_res.has_value()) {
        unexpect_return(fetch_res.error());
    }

    PhyAddr paddr = fetch_res.value();
    size_t order  = static_cast<size_t>(pages2order(frame_count));
    size_t pages   = 1ul << order;
    if (pages > frame_count) {
        add_memory_range<Stage>(paddr + frame_count * PAGESIZE,
                                pages - frame_count);
    }

    loggers::BUDDY::DEBUG("分配了 %u 页物理内存: [%p, %p)",
                          static_cast<unsigned>(frame_count), paddr.addr(),
                          (paddr + frame_count * PAGESIZE).addr());

    auto expand_res = maybe_expand_pool<Stage>();
    if (!expand_res.has_value()) {
        unexpect_return(expand_res.error());
    }
    return paddr;
}

template <KernelStage Stage>
Result<PhyAddr> BuddyAllocator::get_free_pages_in_order(size_t order) {
    if (order > MAX_BUDDY_ORDER) {
        loggers::BUDDY::ERROR("无可用内存块: order %u 超出范围",
                              static_cast<unsigned>(order));
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    return fetch_frame_order<Stage>(order);
}

template <KernelStage Stage>
void BuddyAllocator::put_page(PhyAddr paddr, size_t frame_count) {
    if (!paddr.nonnull() || frame_count == 0) {
        return;
    }

    assert(paddr.aligned<PAGESIZE>());
    add_memory_range<Stage>(paddr, frame_count);
}

template <KernelStage Stage>
void BuddyAllocator::put_page_in_order(PhyAddr paddr, int order) {
    if (!paddr.nonnull()) {
        return;
    }

    assert(order >= 0);
    assert(order <= MAX_BUDDY_ORDER);
    assert(paddr.aligned(block_size_for_order(static_cast<size_t>(order))));

    PhyAddr current_paddr = paddr;
    int current_order     = order;

    while (current_order <= MAX_BUDDY_ORDER) {
        auto node_res = alloc_freeblock<Stage>();
        assert(node_res.has_value());
        FreeBlock *node = node_res.value();
        node->paddr     = current_paddr;
        node->order     = static_cast<size_t>(current_order);
        node->prev      = nullptr;
        node->next      = nullptr;
        link_block(node);

        if (current_order == MAX_BUDDY_ORDER) {
            break;
        }

        FreeBlock *buddy = find_buddy_node(node);
        if (buddy == nullptr) {
            break;
        }

        size_t size         = block_size_for_order(static_cast<size_t>(current_order));
        PhyAddr buddy_paddr = buddy->paddr;
        PhyAddr merged_paddr = buddy_paddr < current_paddr ? buddy_paddr : current_paddr;

        loggers::BUDDY::DEBUG("将 [%p, %p) 与 [%p, %p) 合并为 [%p, %p)",
                              current_paddr.addr(), (current_paddr + size).addr(),
                              buddy_paddr.addr(), (buddy_paddr + size).addr(),
                              merged_paddr.addr(),
                              (merged_paddr + size * 2).addr());

        unlink_block(node);
        unlink_block(buddy);
        free_freeblock(node);
        free_freeblock(buddy);

        current_paddr = merged_paddr;
        ++current_order;
    }
}

template <KernelStage Stage>
Result<PhyAddr> BuddyAllocator::fetch_frame_order(size_t order) {
    size_t current_order = order;
    while (current_order <= MAX_BUDDY_ORDER) {
        if (free_area[current_order] != nullptr) {
            break;
        }
        ++current_order;
    }

    if (current_order > MAX_BUDDY_ORDER) {
        loggers::BUDDY::ERROR("无可用内存块");
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }

    FreeBlock *node = free_area[current_order];
    unlink_block(node);
    PhyAddr paddr = node->paddr;
    free_freeblock(node);

    while (current_order > order) {
        --current_order;
        size_t size        = block_size_for_order(current_order);
        PhyAddr buddy_paddr = paddr + size;
        loggers::BUDDY::DEBUG("将 [%p, %p) 分割为 [%p, %p) 和 [%p, %p)",
                              paddr.addr(), (paddr + (size << 1)).addr(),
                              paddr.addr(), (paddr + size).addr(),
                              buddy_paddr.addr(), (buddy_paddr + size).addr());
        put_page_in_order<Stage>(buddy_paddr, static_cast<int>(current_order));
    }
    return paddr;
}

static_assert(RawGFP<BuddyAllocator>, "Buddy 不满足 RawGFP");
