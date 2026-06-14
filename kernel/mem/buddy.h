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
    static Result<PhyAddr> get_free_page(size_t frame_count);
    static Result<PhyAddr> get_free_pages_in_order(size_t order);
    static void put_page(PhyAddr paddr, size_t frame_count);
    static void put_page_in_order(PhyAddr paddr, int order);
    static void __print_memory_layout() {
        loggers::BUDDY::DEBUG("Buddy Allocator Memory Layout:");
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

    static constexpr size_t FREEBLOCK_HEADER_BLOCKS = 4;
    static constexpr size_t FREEBLOCK_HEADER_BYTES  =
        sizeof(FreeBlock) * FREEBLOCK_HEADER_BLOCKS;

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
    static FreeBlockPool *runtime_pool(PhyAddr paddr) noexcept;
    static FreeBlock *runtime_block(PhyAddr paddr) noexcept;

    static void init_pool0() noexcept;
    static void init_pool(FreeBlockPool *pool, PhyAddr pool_paddr) noexcept;
    static PhyAddr pool_to_pa(const FreeBlockPool *pool) noexcept;
    static FreeBlock *pool_to_blocks(const FreeBlockPool *pool) noexcept;
    static void attach_pool(FreeBlockPool *pool) noexcept;
    static bool pool_needs_expand(const FreeBlockPool *pool) noexcept;
    static Result<void> maybe_expand_pool() noexcept;
    static Result<void> add_new_pool() noexcept;

    static Result<FreeBlock *> alloc_freeblock() noexcept;
    static void free_freeblock(FreeBlock *block) noexcept;

    static void link_block(FreeBlock *node) noexcept;
    static void unlink_block(FreeBlock *node) noexcept;
    static FreeBlock *find_buddy_node(FreeBlock *node) noexcept;

    static Result<PhyAddr> fetch_frame_order(size_t order);
};

static_assert(RawGFP<BuddyAllocator>, "Buddy 不满足 RawGFP");
