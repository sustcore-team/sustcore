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

#include <arch/trait.h>
#include <env.h>
#include <kio.h>
#include <mem/addr.h>
#include <mem/buddy.h>
#include <sus/list.h>
#include <sus/logger.h>
#include <sus/types.h>

#include <cstddef>
#include <new>

util::Defer<BuddyAllocator::BlockList>
    BuddyAllocator::free_area[BuddyAllocator::MAX_BUDDY_ORDER + 1];

void BuddyAllocator::pre_init() {
    // 初始化空闲块链表
    for (int i = 0; i <= BuddyAllocator::MAX_BUDDY_ORDER; i++) {
        BuddyAllocator::free_area[i].construct();
    }

    auto &meminfo = env::inst().meminfo();
    for (size_t i = 0; i < meminfo.region_cnt; ++i) {
        const MemRegion &region = meminfo.regions[i];
        if (region.status == MemRegion::MemoryStatus::FREE) {
            PhyAddr start_addr = region.ptr.page_align_up();
            PhyAddr end_addr   = (region.ptr + region.size).page_align_down();
            size_t pages       = (end_addr - start_addr) / PAGESIZE;
            if (pages > 0) {
                loggers::BUDDY::DEBUG("添加可用内存区域 [%p, %p), 共 %d 页",
                             start_addr.addr(), end_addr.addr(), pages);
                add_memory_range<KernelStage::PRE_INIT>(start_addr, pages);
            }
        }
    }
}

void BuddyAllocator::post_init() {
    loggers::BUDDY::DEBUG("进入 BuddyAllocator::post_init, 迁移空闲块链表到KPA空间");

    // 先进行一次遍历, 打印迁移前的链表状态
    loggers::BUDDY::DEBUG("Buddy Allocator Memory Layout:\n");
    for (int i = 0; i <= MAX_BUDDY_ORDER; i++) {
        BlockList &list = free_area[i].get();
        auto *sentinel_pa = convert<PhyAddr>((KvaAddr)&list.sentinel()).addr();
        loggers::BUDDY::DEBUG("Order %d: %d blocks:", i, list.size());
        for (auto iter = list.begin(); iter != list.end(); ++iter) {
            auto *nxt = iter.operator->();

            if (nxt == sentinel_pa) {
                break;
            }

            PhyAddr paddr = block2pa<KernelStage::PRE_INIT>(&*iter);
            loggers::BUDDY::DEBUG("    Free block at [%p, %p)\n", i, paddr.addr(),
                         (paddr + (1ul << (i + 12))).addr());
        }
    }

    for (int i = 0; i <= BuddyAllocator::MAX_BUDDY_ORDER; i++) {
        BlockList &list = free_area[i].get();

        // 哨兵节点
        // 将其从 KA 转换为 PA, 再从 PA 转换回 KPA
        // 这样做是为了确保链表中的所有指针都指向 Kernel Physical Memory Space
        KvaAddr sentinel_kva = (KvaAddr)&list.sentinel();
        FreeBlock *sentinel  = convert<KpaAddr>(sentinel_kva).as<FreeBlock>();

        // 开始遍历
        FreeBlock *iter = sentinel;
        // 一开始就是哨兵节点,
        do {
            // iter 现在是 KPA 指针
            // iter->next 和 iter->prev 在内存中仍然是 PA
            // 需要转换到KPA
            PhyAddr next_pa = (PhyAddr)iter->list_head.next;
            PhyAddr prev_pa = (PhyAddr)iter->list_head.prev;

            FreeBlock *next_ka = convert<KpaAddr>(next_pa).as<FreeBlock>();
            FreeBlock *prev_ka = convert<KpaAddr>(prev_pa).as<FreeBlock>();

            // 写回
            iter->list_head.next =
                next_ka == sentinel ? &list.sentinel() : next_ka;
            iter->list_head.prev =
                prev_ka == sentinel ? &list.sentinel() : prev_ka;

            loggers::BUDDY::DEBUG("next PA: %p -> KPA: %p, prev PA: %p -> KPA: %p",
                         next_pa.addr(), iter->list_head.next, prev_pa.addr(),
                         iter->list_head.prev);

            // 迭代
            iter = iter->list_head.next;
            // 直到到达哨兵节点
        } while (iter != &list.sentinel());
    }

    __print_memory_layout<KernelStage::POST_INIT>();
    loggers::BUDDY::INFO("BuddyAllocator initialized and migrated to KVA.");
}