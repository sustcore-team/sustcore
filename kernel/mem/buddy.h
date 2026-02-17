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

// #pragma once

// #include <arch/trait.h>
// #include <event/event.h>
// #include <mem/gfp_def.h>
// #include <stddef.h>
// #include <sus/defer.h>
// #include <sus/list.h>
// #include <sus/types.h>

// class BuddyAllocator {
// public:
//     struct FreeBlock {
//         util::ListHead<FreeBlock> list_head;

//         FreeBlock() : list_head({}) {}
//     };

//     static constexpr int MAX_BUDDY_ORDER = 15;

//     static void pre_init(MemRegion *regions, size_t region_count);

//     static void post_init();

//     /**
//      * @brief 分配单个页
//      *
//      * @return void*
//      */
//     static void *get_free_page();

//     /**
//      * @brief 分配多个页
//      *
//      * @param frame_count
//      * @return void*
//      */
//     static void *get_free_page(size_t frame_count);

//     /**
//      * @brief 按order阶数分配
//      *
//      * @param order
//      * @return void*
//      */
//     static void *get_free_pages_in_order(size_t order);

//     /**
//      * @brief 释放单个页
//      *
//      * @param ptr
//      */
//     static void put_page(void *ptr);

//     /**
//      * @brief 释放多个页
//      *
//      * @param ptr
//      * @param frame_count
//      */
//     static void put_page(void *ptr, size_t frame_count);

//     /**
//      * @brief 按order阶数释放页
//      *
//      * @param ptr
//      * @param order
//      */
//     static void put_page_in_order(void *ptr, int order);

// private:
//     using BlockList = util::IntrusiveList<FreeBlock>;
//     static util::Defer<BlockList> free_area[MAX_BUDDY_ORDER + 1];
//     /**
//      * @brief 页数转换为order阶数
//      *
//      * @param count
//      * @return int
//      */
//     static int pages2order(size_t count);

//     /**
//      * @brief 物理地址转换为FreeBlock指针
//      *
//      * @param paddr
//      * @return FreeBlock*
//      */
//     static FreeBlock *pa2block(const umb_t paddr);

//     /**
//      * @brief FreeBlock指针转换为物理地址
//      *
//      * @param block
//      * @return void*
//      */
//     static void *block2pa(FreeBlock *block);

//     /**
//      * @brief 按order阶数分配内存块
//      *
//      * @param order
//      * @return void*
//      */
//     static void *fetch_frame_order(size_t order);

//     friend class BuddyListener;
// };

// static_assert(GFPTrait<BuddyAllocator>,
//               "Buddy 不满足 GFPTrait");
