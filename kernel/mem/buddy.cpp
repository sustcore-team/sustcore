/**
 * @file buddy.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2026-01-29
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <basecpp/logger.h>
#include <kio.h>
#include <mem/buddy.h>
#include <mem/kaddr.h>

#include <cstddef>

#include "arch/trait.h"
#include "sus/list.h"
#include "sus/types.h"

inline void *operator new(size_t, void *p) noexcept {
    return p;
}

util::IntrusiveList<BuddyAllocator::FreeBlock>
    BuddyAllocator::free_area[BuddyAllocator::MAX_BUDDY_ORDER + 1];

/**
 * @brief 按页数添加一段物理内存范围到Buddy分配器
 *
 * @param paddr
 * @param pages
 */
static void add_memory_range(void *paddr, size_t pages) {
    umb_t addr    = (umb_t)paddr;
    size_t remain = pages;

    while (remain > 0) {
        size_t order = 0;
        while (order <= BuddyAllocator::MAX_BUDDY_ORDER) {
            size_t try_pages = 1UL << (order + 1);
            size_t try_size  = try_pages << 12;

            if (try_pages <= remain && (addr % try_size) == 0) {
                order++;
            } else {
                break;
            }
        }

        BuddyAllocator::free_frame((void *)addr, 1UL << order);

        size_t block_pages  = 1UL << order;
        addr               += block_pages << 12;
        remain             -= block_pages;
    }
}

void BuddyAllocator::pre_init(MemRegion *regions, size_t region_count) {
    for (size_t i = 0; i <= MAX_BUDDY_ORDER; ++i) {
        free_area[i] = util::IntrusiveList<FreeBlock>();
    }

    for (size_t i = 0; i < region_count; ++i) {
        MemRegion &region = regions[i];
        if (region.status == MemRegion::MemoryStatus::FREE) {
            umb_t start_addr = (umb_t)region.ptr;
            umb_t end_addr   = (umb_t)region.ptr + region.size;

            // 对齐到页边界
            if (start_addr % 0x1000 != 0) {
                start_addr += 0x1000 - (start_addr % 0x1000);
            }
            if (end_addr % 0x1000 != 0) {
                end_addr -= end_addr % 0x1000;
            }

            size_t pages = (end_addr - start_addr) / 0x1000;
            if (pages > 0) {
                add_memory_range((void *)start_addr, pages);
                BUDDY.DEBUG("添加可用内存区域 [%p, %p), 共 %d 页",
                            (void *)start_addr, (void *)end_addr, pages);
            }
        }
    }
}

void BuddyAllocator::post_init() {
    BUDDY.DEBUG("enter post_init");

    for (int i = 0; i <= BuddyAllocator::MAX_BUDDY_ORDER; i++) {
        auto &list = free_area[i];

        // 哨兵节点
        // 将其从 KA 转换为 PA, 再从 PA 转换回 KPA
        // 这样做是为了确保链表中的所有指针都指向 Kernel Physical Memory Space
        FreeBlock *sentinel = (FreeBlock *)PA2KPA(KA2PA(&list.sentinel()));
        // 开始遍历
        FreeBlock *iter = sentinel;
        
        // 一开始就是哨兵节点, 
        do {
            // iter 现在是 KPA 指针
            // iter->next 和 iter->prev 在内存中仍然是 PA
            // 需要转换到KPA
            FreeBlock *next_pa = iter->next;
            FreeBlock *prev_pa = iter->prev;

            FreeBlock *next_ka = (FreeBlock *)PA2KPA(next_pa);
            FreeBlock *prev_ka = (FreeBlock *)PA2KPA(prev_pa);

            BUDDY.DEBUG("next PA: %p -> KPA: %p, prev PA: %p -> KPA: %p",
                        next_pa, next_ka, prev_pa, prev_ka);

            // 写回
            iter->next = next_ka;
            iter->prev = prev_ka;

            // 迭代
            iter = iter->next;
        // 直到到达哨兵节点
        } while (iter != sentinel);
    }
    BUDDY.INFO("BuddyAllocator initialized and migrated to KVA.");
}

void BuddyAllocator::free_frame(void *ptr, size_t frame_count) {
    if (ptr == nullptr)
        return;

    umb_t paddr  = (umb_t)ptr;
    size_t order = pages2order(frame_count);

    if (paddr >= (umb_t)KPHY_VA_OFFSET) {
        BUDDY.ERROR("free_frame: 地址 %p 超出物理地址范围", ptr);
        return;
    }
    if ((paddr % 0x1000) != 0) {
        BUDDY.ERROR("free_frames_in_order: 地址 %p 非页对齐", paddr);
        return;
    }
    if (order < 0 || order > MAX_BUDDY_ORDER) {
        BUDDY.ERROR("free_frames_in_order: 无效的页数 2^%d", order);
        return;
    }

    free_frame_in_order(ptr, order);
}

void BuddyAllocator::free_frame_in_order(void *ptr, int order) {
    if (order < 0 || order > MAX_BUDDY_ORDER) {
        BUDDY.ERROR("free_frame_in_order: 无效的order %d", order);
        return;
    }

    umb_t paddr = (umb_t)ptr;

    while (order <= BuddyAllocator::MAX_BUDDY_ORDER) {
        FreeBlock *block_kva = pa2block(paddr);

        // 在指定的内存地址 address 上构造一个 Type 类型的对象
        FreeBlock *node = new (block_kva) FreeBlock();

        auto &list = free_area[order];

        // 插入到有序链表中
        auto it = list.begin();
        while (it != list.end()) {
            umb_t node_pa = (umb_t)block2pa(&*it);
            if (node_pa > paddr) {
                break;
            }
            ++it;
        }

        auto inserted_it = list.insert(it, *node);

        if (order == BuddyAllocator::MAX_BUDDY_ORDER) {
            break;
        }

        umb_t block_size = 1UL << (order + 12);

        // 判断当前块是左半边还是右半边
        bool is_left = !((paddr >> (order + 12)) & 1);

        FreeBlock *buddy = nullptr;

        if (is_left) {
            auto next = inserted_it;
            ++next;
            // 检查是否存在且物理地址连续
            if (next != list.end()) {
                umb_t next_pa = (umb_t)block2pa(&*next);
                if (next_pa == paddr + block_size) {
                    buddy = &*next;
                }
            }
        } else {
            // 检查是否存在且物理地址连续
            if (inserted_it != list.begin()) {
                auto prev = inserted_it;
                --prev;
                umb_t prev_pa = (umb_t)block2pa(&*prev);
                if (prev_pa == paddr - block_size) {
                    buddy = &*prev;
                }
            }
        }

        if (buddy) {
            umb_t buddy_paddr  = (umb_t)block2pa(buddy);
            umb_t merged_paddr = is_left ? paddr : paddr - block_size;
            BUDDY.DEBUG("将 [%p, %p) 与 [%p, %p) 合并为 [%p, %p)",
                        (void *)paddr, (void *)(paddr + block_size),
                        (void *)buddy_paddr, (void *)(buddy_paddr + block_size),
                        (void *)merged_paddr,
                        (void *)(merged_paddr + block_size * 2));

            // 从链表中移除node和buddy
            list.remove(*node);
            list.remove(*buddy);

            paddr = is_left ? paddr : paddr - block_size;
            order++;
        } else {
            break;
        }
    }
}

void BuddyAllocator::free_frame(void *ptr) {
    free_frame(ptr, 1);
}

void *BuddyAllocator::alloc_frame(size_t frame_count) {
    if (frame_count <= 0 ||
        (frame_count >> BuddyAllocator::MAX_BUDDY_ORDER) > 1)
    {
        BUDDY.ERROR("无效的页数 %d", frame_count);
        return nullptr;
    }

    size_t order = pages2order(frame_count);
    umb_t paddr  = (umb_t)fetch_frame_order(order);

    // 归还多余部分
    size_t allocated_pages = 1ul << order;
    if (allocated_pages > frame_count) {
        umb_t remain_addr   = paddr + (frame_count << 12);
        size_t remain_pages = allocated_pages - frame_count;

        add_memory_range((void *)remain_addr, remain_pages);
    }

    return (void *)paddr;
}

void *BuddyAllocator::alloc_frames_in_order(size_t order) {
    if (order > BuddyAllocator::MAX_BUDDY_ORDER) {
        BUDDY.ERROR("无可用内存块: order %d 超出范围", order);
        return nullptr;
    }
    return fetch_frame_order(order);
}

void *BuddyAllocator::fetch_frame_order(size_t order) {
    size_t current_order = order;

    while (current_order <= BuddyAllocator::MAX_BUDDY_ORDER) {
        auto &list = free_area[current_order];
        if (!list.empty()) {
            break;
        }
        current_order++;
    }

    if (current_order > BuddyAllocator::MAX_BUDDY_ORDER) {
        // 无可用内存块
        BUDDY.ERROR("无可用内存块");
        return nullptr;
    }

    auto &list      = free_area[current_order];
    FreeBlock &node = list.front();
    umb_t paddr     = (umb_t)block2pa(&node);

    list.pop_front();

    while (current_order > order) {
        current_order--;

        umb_t block_size  = 1ul << (current_order + 12);
        umb_t buddy_paddr = paddr + block_size;

        free_frame((void *)buddy_paddr, 1ul << current_order);

        // paddr 保持指向左半部分，继续下一轮分裂或结束
    }
    return (void *)paddr;
}

void *BuddyAllocator::alloc_frame() {
    return alloc_frame(1);
}

int BuddyAllocator::pages2order(size_t count) {
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

BuddyAllocator::FreeBlock *BuddyAllocator::pa2block(const umb_t paddr) {
    return reinterpret_cast<BuddyAllocator::FreeBlock *>(PA2KPA((void *)paddr));
}

void *BuddyAllocator::block2pa(BuddyAllocator::FreeBlock *block) {
    return KPA2PA(reinterpret_cast<void *>(block));
}
