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
#include <event/event.h>
#include <kio.h>
#include <mem/addr.h>
#include <mem/gfp_def.h>
#include <stddef.h>
#include <sus/defer.h>
#include <sus/list.h>
#include <sus/types.h>
#include <cstdio>

class BuddyAllocator {
public:
    struct FreeBlock {
        util::ListHead<FreeBlock> list_head;

        FreeBlock() : list_head({}) {}
    };

    struct FreeBlockCmp {
        bool operator()(const FreeBlock &a, const FreeBlock &b) const {
            return &a < &b;
        }
    };

    static constexpr int MAX_BUDDY_ORDER = 15;

    static void pre_init(MemRegion *regions, size_t region_count);

    static void post_init(MemRegion *regions, size_t region_count);

    /**
     * @brief 分配多个页
     *
     * @param frame_count
     * @return PhyAddr
     */
    template <KernelStage Stage = KernelStage::POST_INIT>
    static PhyAddr get_free_page(size_t frame_count = 1);

    /**
     * @brief 按order阶数分配
     *
     * @param order
     * @return void*
     */
    template <KernelStage Stage = KernelStage::POST_INIT>
    static PhyAddr get_free_pages_in_order(size_t order);

    /**
     * @brief 释放多个页
     *
     * @param paddr
     * @param frame_count
     */
    template <KernelStage Stage = KernelStage::POST_INIT>
    static void put_page(PhyAddr paddr, size_t frame_count = 1);

    /**
     * @brief 按order阶数释放页
     *
     * @param paddr
     * @param order
     */
    template <KernelStage Stage = KernelStage::POST_INIT>
    static void put_page_in_order(PhyAddr paddr, int order);

private:
    using BlockList = util::OrderedIntrusiveList<FreeBlock, &FreeBlock::list_head, FreeBlockCmp>;
    static util::Defer<BlockList> free_area[MAX_BUDDY_ORDER + 1];
    /**
     * @brief 按页数添加一段物理内存范围到Buddy分配器
     *
     * @param paddr
     * @param pages
     */
    template <KernelStage Stage>
    static void add_memory_range(const PhyAddr paddr, const size_t pages);
    /**
     * @brief 页数转换为order阶数
     *
     * @param count
     * @return int
     */
    static constexpr int pages2order(size_t count) {
        switch (count) {
            case 1:  return 0;
            case 2:  return 1;
            case 3:
            case 4:  return 2;
            default: {
                size_t order = 3;
                while (order <= BuddyAllocator::MAX_BUDDY_ORDER) {
                    if ((1ul << order) >= count) {
                        break;
                    }
                    order++;
                }
                return order;
            }
        }
    }

    /**
     * @brief 物理地址转换为FreeBlock指针
     *
     * @param paddr
     * @return FreeBlock*
     */
    template <KernelStage Stage>
    static FreeBlock *pa2block(const PhyAddr paddr) {
        using StageAddr = _StageAddr<Stage>;
        return convert<StageAddr>(paddr).template as<FreeBlock>();
    }

    /**
     * @brief FreeBlock指针转换为物理地址
     *
     * @param block
     * @return void*
     */
    template <KernelStage Stage>
    static PhyAddr block2pa(FreeBlock *block) {
        using StageAddr      = _StageAddr<Stage>;
        StageAddr block_addr = (StageAddr)block;
        return convert<PhyAddr>(block_addr);
    }

    /**
     * @brief 按order阶数分配内存块
     *
     * @param order
     * @return void*
     */
    template <KernelStage Stage>
    static PhyAddr fetch_frame_order(size_t order) {
        // 寻找第一个非空链表
        size_t current_order = order;
        while (current_order <= BuddyAllocator::MAX_BUDDY_ORDER) {
            BlockList &list = free_area[current_order].get();
            if (!list.empty()) {
                break;
            }
            current_order++;
        }

        if (current_order > BuddyAllocator::MAX_BUDDY_ORDER) {
            // 无可用内存块
            BUDDY::ERROR("无可用内存块");
            return PhyAddr::null;
        }

        // 从链表头部取出一个内存块
        BlockList &list = free_area[current_order].get();
        FreeBlock &node = list.front();
        PhyAddr paddr   = block2pa<Stage>(&node);
        list.pop_front();

        // 如果分配的块大于请求的块，则将剩余部分重新放回链表
        while (current_order > order) {
            current_order--;

            umb_t block_size    = 1ul << (current_order + 12);
            PhyAddr buddy_paddr = paddr + block_size;

            BUDDY::DEBUG("将 [%p, %p) 分割为 [%p, %p) 和 [%p, %p)",
                         paddr.addr(), (paddr + (block_size << 1)).addr(),
                         paddr.addr(), (paddr + block_size).addr(),
                         buddy_paddr.addr(), (buddy_paddr + block_size).addr());

            put_page<Stage>(buddy_paddr, 1ul << current_order);

            // paddr 保持指向左半部分，继续下一轮分裂或结束
        }
        return paddr;
    }

    friend class BuddyListener;
};

template <KernelStage Stage>
void BuddyAllocator::add_memory_range(const PhyAddr paddr, const size_t pages) {
    size_t remain = pages;
    PhyAddr addr  = paddr;

    while (remain > 0) {
        size_t order = 0;
        while (order < BuddyAllocator::MAX_BUDDY_ORDER) {
            size_t try_pages = 1UL << (order + 1);
            size_t try_size  = try_pages << 12;

            if (try_pages <= remain && addr.aligned(try_size)) {
                order++;
            } else {
                break;
            }
        }
        put_page_in_order<Stage>(addr, order);

        size_t block_pages  = 1UL << order;
        addr               += block_pages << 12;
        remain             -= block_pages;
    }
}

template <KernelStage Stage>
PhyAddr BuddyAllocator::get_free_page(size_t frame_count) {
    assert(frame_count > 0);
    assert(frame_count <= (1ul << MAX_BUDDY_ORDER));
    const size_t order  = pages2order(frame_count);
    const PhyAddr paddr = fetch_frame_order<Stage>(order);

    // 归还多余部分
    const size_t allocated_pages = 1ul << order;
    if (allocated_pages > frame_count) {
        const PhyAddr remain_addr = paddr + frame_count * PAGESIZE;
        const size_t remain_pages = allocated_pages - frame_count;

        add_memory_range<Stage>(remain_addr, remain_pages);
    }

    return paddr;
}

template <KernelStage Stage>
PhyAddr BuddyAllocator::get_free_pages_in_order(size_t order) {
    if (order > BuddyAllocator::MAX_BUDDY_ORDER) {
        BUDDY::ERROR("无可用内存块: order %d 超出范围", order);
        return PhyAddr::null;
    }
    return fetch_frame_order<Stage>(order);
}

template <KernelStage Stage>
void BuddyAllocator::put_page(PhyAddr paddr, size_t frame_count) {
    if (!paddr.nonnull())
        return;
    size_t order = pages2order(frame_count);

    assert(paddr.aligned<PAGESIZE>());
    assert(order >= 0);
    assert(order <= MAX_BUDDY_ORDER);

    put_page_in_order<Stage>(paddr, order);
}

template <KernelStage Stage>
void BuddyAllocator::put_page_in_order(const PhyAddr paddr, int order) {
    assert (order >= 0);
    assert (order <= MAX_BUDDY_ORDER);
    umb_t block_size = 1UL << (order + 12);
    assert (paddr.aligned(block_size));

    PhyAddr cur_paddr = paddr;

    while (order <= BuddyAllocator::MAX_BUDDY_ORDER) {
        // 在指定的内存地址 address 上构造一个 Type 类型的对象
        FreeBlock *block_kva = pa2block<Stage>(cur_paddr);
        FreeBlock *node = new (block_kva) FreeBlock();

        // 插入到有序链表中
        BlockList &list = free_area[order].get();

        // 插入到有序链表中
        auto inserted_it = list.insert(*node);
        if (order == BuddyAllocator::MAX_BUDDY_ORDER) {
            break;
        }

        // 否则, 尝试合并
        // 判断当前块是左半边还是右半边
        umb_t block_size = 1UL << (order + 12);
        bool is_left = !((cur_paddr.arith() >> (order + 12)) & 1);

        FreeBlock *buddy = nullptr;

        // 左侧
        if (is_left) {
            auto next = inserted_it;
            ++next;
            // 检查是否存在且物理地址连续
            if (next != list.end()) {
                PhyAddr next_pa = block2pa<Stage>(&*next);
                if (next_pa == cur_paddr + block_size) {
                    buddy = &*next;
                }
            }
        } else {
            // 检查是否存在且物理地址连续
            if (inserted_it != list.begin()) {
                auto prev = inserted_it;
                --prev;
                assert(prev != list.end());
                PhyAddr prev_pa = block2pa<Stage>(&*prev);
                if (prev_pa == cur_paddr - block_size) {
                    buddy = &*prev;
                }
            }
        }

        if (buddy) {
            PhyAddr buddy_paddr  = block2pa<Stage>(buddy);
            PhyAddr merged_paddr = is_left ? cur_paddr : cur_paddr - block_size;
            BUDDY::DEBUG("将 [%p, %p) 与 [%p, %p) 合并为 [%p, %p)",
                         cur_paddr.addr(), (cur_paddr + block_size).addr(),
                         buddy_paddr.addr(), (buddy_paddr + block_size).addr(),
                         merged_paddr.addr(),
                         (merged_paddr + block_size * 2).addr());

            // 从链表中移除node和buddy
            list.remove(*node);
            list.remove(*buddy);

            cur_paddr = merged_paddr;
            order++;
        } else {
            break;
        }
    }
}

static_assert(GFPTrait<BuddyAllocator>, "Buddy 不满足 GFPTrait");
