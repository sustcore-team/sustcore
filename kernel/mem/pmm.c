/**
 * @file pmm.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 物理内存管理器实现
 * @version alpha-1.0.0
 * @date 2025-11-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <mem/alloc.h>
#include <mem/kmem.h>
#include <mem/pmm.h>

#ifdef DLOG_PMM
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

/**
 * @brief Buddy System空闲内存块结构体
 *
 * 使用有序双向链表
 *
 * 从head->tail按照指向的内存地址升序排列
 *
 */
typedef struct __PMMFreeArea__ {
    umb_t order;                   // 内存块阶数
    void *paddr;                   // 该空闲内存块的物理地址
    struct __PMMFreeArea__ *prev;  // 指向上一个空闲内存块
    struct __PMMFreeArea__ *next;  // 指向下一个空闲内存块
} PMMFreeArea;

#define MAX_BUDDY_ORDER 15  // 最大伙伴阶数 (2^15 * 4KB = 128MB)

/**
 * @brief Buddy System空闲内存块链表头
 *
 * free_area_head[order] 指向阶数为order的空闲内存块链表头
 * 其代表 2^order 个连续空闲页框
 *
 * 从head->tail按照指向的内存地址升序排列
 *
 */
static PMMFreeArea *free_area_head[MAX_BUDDY_ORDER + 1] = {};

/**
 * @brief Buddy System空闲内存块链表尾
 *
 * free_area_tail[order] 指向阶数为order的空闲内存块链表尾
 * 其代表 2^order 个连续空闲页框
 *
 * 从head->tail按照指向的内存地址升序排列
 *
 */
static PMMFreeArea *free_area_tail[MAX_BUDDY_ORDER + 1] = {};

// farl: Free area list

/**
 * @brief 初始化FARL链表
 *
 * @param order 初始化order阶FARL链表
 */
static void init_farl(int order) {
    free_area_head[order] = nullptr;
    free_area_tail[order] = nullptr;
}

// 尝试merge相邻的两个空闲内存块
static PMMFreeArea *farl_try_merge(PMMFreeArea *area) {
    if (area->order == MAX_BUDDY_ORDER) {
        // 已经是最大阶数, 无法合并
        return nullptr;
    }

    // 判断当前area是否是2^(order + 1)(页)对齐的
    // 是则向后合并 ; 否则向前合并

    const umb_t block_size = 1ul << (area->order + 12);
    bool forward           = (((umb_t)area->paddr) % ((block_size << 1))) != 0;
    // 向前合并
    if (forward) {
        // 前驱
        PMMFreeArea *prev = area->prev;

        if (prev == nullptr) {
            // 无前驱, 无法合并
            return nullptr;
        }

        // 判断前驱是否相邻
        bool neighbor = (area->paddr - prev->paddr) == block_size;
        if (neighbor) {
            // 从链表中移除area和prev
            // prev2 -> prev -> area -> next
            PMMFreeArea *prev2 = prev->prev;
            PMMFreeArea *next  = area->next;

            // 分类讨论
            // prev2 -> prev -> area -> next 与
            // null -> prev -> area -> next  与
            // prev2 -> prev -> area -> null 与
            // null -> prev -> area -> null

            if (prev2 != nullptr && next != nullptr) {
                // prev2 -> prev -> area -> next 变为
                // prev2 -> next
                prev2->next = next;
                next->prev  = prev2;
            } else if (prev2 == nullptr && next != nullptr) {
                // null -> prev -> area -> next 变为
                // head = next
                next->prev                  = nullptr;
                free_area_head[area->order] = next;
            } else if (prev2 != nullptr && next == nullptr) {
                // prev2 -> prev -> area -> null 变为
                // tail = prev2
                prev2->next                 = nullptr;
                free_area_tail[area->order] = prev2;
            } else {
                // null -> prev -> area -> null 变为
                // null
                free_area_head[area->order] = nullptr;
                free_area_tail[area->order] = nullptr;
            }

            log_debug(
                "将 [%p, %p) 与 [%p, %p) 合并为 [%p, %p)", prev->paddr,
                (void *)((umb_t)prev->paddr + (1ul << (prev->order + 12))),
                area->paddr,
                (void *)((umb_t)area->paddr + (1ul << (area->order + 12))),
                prev->paddr,
                (void *)((umb_t)area->paddr + (1ul << (area->order + 12))));

            // 释放area
            kfree(area);

            // 将prev提升一个阶数
            prev->order += 1;
            // forward时
            // merged->paddr = prev->paddr

            // 无需重新设置prev的paddr
            // 返回prev指针, 退出至最外层再尝试合并
            // 以避免递归调用
            return prev;
        }
    } else {
        // 向后合并
        PMMFreeArea *next = area->next;
        if (next == nullptr) {
            // 无后继, 无法合并
            return nullptr;
        }

        // 判断后继是否相邻
        bool neighbor = (next->paddr - area->paddr) == block_size;
        if (neighbor) {
            // 从链表中移除area和next
            // prev -> area -> next -> next2
            PMMFreeArea *prev  = area->prev;
            PMMFreeArea *next2 = next->next;
            // 分类讨论
            // prev -> area -> next -> next2 与
            // null -> area -> next -> next2  与
            // prev -> area -> next -> null 与
            // null -> area -> next -> null
            if (prev != nullptr && next2 != nullptr) {
                // prev -> area -> next -> next2 变为
                // prev -> next2
                prev->next  = next2;
                next2->prev = prev;
            } else if (prev == nullptr && next2 != nullptr) {
                // null -> area -> next -> next2 变为
                // head = next2
                next2->prev                 = nullptr;
                free_area_head[area->order] = next2;
            } else if (prev != nullptr && next2 == nullptr) {
                // prev -> area -> next -> null 变为
                // tail = prev
                prev->next                  = nullptr;
                free_area_tail[area->order] = prev;
            } else {
                // null -> area -> next -> null 变为
                // null
                free_area_head[area->order] = nullptr;
                free_area_tail[area->order] = nullptr;
            }

            log_debug(
                "将 [%p, %p) 与 [%p, %p) 合并为 [%p, %p)", area->paddr,
                (void *)((umb_t)area->paddr + (1ul << (area->order + 12))),
                next->paddr,
                (void *)((umb_t)next->paddr + (1ul << (next->order + 12))),
                area->paddr,
                (void *)((umb_t)next->paddr + (1ul << (next->order + 12))));

            // 释放next
            kfree(next);

            // 将area提升一个阶数
            area->order += 1;
            // forward时
            // merged->paddr = area->paddr

            // 无需重新设置area的paddr
            // 返回area指针, 退出至最外层再尝试合并
            // 以避免递归调用
            return area;
        }
    }

    return nullptr;
}

/**
 * @brief 向FARL中添加一个空闲内存块
 *
 * @param order 阶数
 * @param paddr 物理地址
 * @return PMMFreeArea* 若不为空, 则应继续尝试合并
 */
static PMMFreeArea *farl_insert(PMMFreeArea *new_area) {
    // 设置相关字段
    umb_t order = new_area->order;
    void *paddr = new_area->paddr;

    if (free_area_head[order] == nullptr) {
        // 链表为空
        free_area_head[order] = new_area;
        free_area_tail[order] = new_area;
        new_area->prev        = nullptr;
        new_area->next        = nullptr;
        return nullptr;
    }

    // 寻找应该要插入的位置
    PMMFreeArea *cur = free_area_head[order];

    // 寻找第一个大于paddr的节点
    while (cur != nullptr && cur->paddr < paddr) {
        cur = cur->next;
    }

    if (cur == nullptr) {
        // 插入到链表尾
        PMMFreeArea *tail     = free_area_tail[order];
        tail->next            = new_area;
        new_area->prev        = tail;
        new_area->next        = nullptr;
        free_area_tail[order] = new_area;
        return farl_try_merge(new_area);
    }

    PMMFreeArea *prev = cur->prev;

    // 如果prev为nullptr
    if (prev == nullptr) {
        // 插入到链表头
        PMMFreeArea *head     = free_area_head[order];
        head->prev            = new_area;
        new_area->next        = head;
        new_area->prev        = nullptr;
        free_area_head[order] = new_area;
        return farl_try_merge(new_area);
    }

    // 则从
    // prev -> cur(prev <- cur)
    // 变为
    // prev -> new_area -> cur(prev <- new_area <- cur)
    prev->next     = new_area;
    new_area->next = cur;

    new_area->prev = prev;
    cur->prev      = new_area;
    return farl_try_merge(new_area);
}

/**
 * @brief 添加一个空闲内存块到物理内存管理器
 *
 * @param paddr 物理地址
 * @param order 阶数
 * @return true 添加成功
 * @return false 添加失败
 */
static bool pmm_add_free_memblock(void *paddr, int order) {
    // 判断是否 2^order(页) 对齐
    const umb_t block_size = 1ul << (order + 12);
    if (((umb_t)paddr % block_size) != 0) {
        log_error("pmm_add_free_memblock: 地址 %p 非 %lu 对齐", paddr,
                  block_size);
        return false;
    }

    log_debug("添加了内存块 [%p, %p), 阶数=%d", paddr,
              (void *)((umb_t)paddr + block_size), order);

    // 构造FreeArea对象记录
    PMMFreeArea *area = kmalloc(sizeof(PMMFreeArea));
    area->order       = order;
    area->paddr       = paddr;
    area->prev        = nullptr;
    area->next        = nullptr;

    PMMFreeArea *merged = farl_insert(area);
    while (merged != nullptr) {
        merged = farl_insert(merged);
    }
    return true;
}

/**
 * @brief 添加多个空闲页到物理内存管理器
 *
 * @param paddr 物理地址
 * @param pages 页数
 * @return true 添加成功
 * @return false 添加失败
 */
static bool pmm_add_free_pages(void *paddr, int pages) {
    // 将其分解为多个2^order页的内存块
    // 每个内存块都是 2^order 页对齐的

    umb_t addr = (umb_t)paddr;
    int remain = pages;

    // 处理思路: 选取一个尽可能大的order
    // 该order满足: 目前的addr是2^order页对齐的, 且不超过remain页数
    // 将
    while (remain > 0) {
        // 找到当前地址下最大可用阶数
        // order = 0一定能成功, 所以无需处理
        int order = 1;
        while (order <= MAX_BUDDY_ORDER) {
            // 计算一块中的页数与大小
            umb_t block_pages = 1ul << order;
            umb_t block_size  = block_pages << 12;
            // 地址对齐且不超过剩余页数
            if ((addr % block_size) == 0 && block_pages <= remain) {
                order++;
            } else {
                break;
            }
        }
        // 当break时, 至少上一个order是可用的
        order--;
        // 插入该阶数的块(addr, addr + 2^order * 4KB - 1)
        if (!pmm_add_free_memblock((void *)addr, order)) {
            log_error("pmm_add_free_pages: 添加内存块失败 addr=%p order=%d",
                      (void *)addr, order);
            return false;
        }
        umb_t block_pages  = 1ul << order;
        // 处理(addr + 2^order * 4KB, addr + pages * 4KB - 1)部分
        addr              += block_pages << 12;
        // 减少剩余需要加入的块
        remain            -= block_pages;
    }
    return true;
}

/**
 * @brief 从物理内存管理器中获取一个空闲内存块
 *
 * @param order 内存块阶数
 * @return void* 内存块地址
 */
static void *pmm_fetch_free_memblock(int order) {
    void *paddr = nullptr;

    // 无可用内存块
    if (free_area_head[order] == nullptr) {
        // 已经是最大阶数, 无法分配
        if (order == MAX_BUDDY_ORDER) {
            log_error("无可用内存块!内存已满!");
            return nullptr;
        }
        // 向高一阶请求内存块
        void *higher_block = pmm_fetch_free_memblock(order + 1);
        if (higher_block == nullptr) {
            // 高一阶无可用内存块
            return nullptr;
        }

        // 将高一阶内存块拆分为两个低一阶内存块
        umb_t block_size = 1ul << (order + 12);
        void *buddy1     = higher_block;
        void *buddy2     = (void *)((umb_t)higher_block + block_size);

        log_debug("将[%p, %p)拆分为[%p, %p)和[%p, %p)", higher_block,
                  (void *)((umb_t)higher_block + block_size * 2), buddy1,
                  (void *)((umb_t)buddy1 + block_size), buddy2,
                  (void *)((umb_t)buddy2 + block_size));

        // 留下buddy2, 返回buddy1
        paddr = buddy1;
        pmm_add_free_memblock(buddy2, order);
    } else {
        // 从链表头取出一个内存块
        PMMFreeArea *area = free_area_head[order];
        paddr             = area->paddr;

        // 从链表中移除area
        PMMFreeArea *next = area->next;
        if (next != nullptr) {
            next->prev            = nullptr;
            free_area_head[order] = next;
        } else {
            // 只有一个节点
            free_area_head[order] = nullptr;
            free_area_tail[order] = nullptr;
        }

        // 释放area结构体
        kfree(area);
    }

    log_debug("分配了内存块 [%p, %p), 阶数=%d", paddr,
              (void *)((umb_t)paddr + (1ul << (order + 12))), order);

    return paddr;
}

void *alloc_page(void) {
    return pmm_fetch_free_memblock(0);
}

void *alloc_pages_in_order(int order) {
    if (order > MAX_BUDDY_ORDER) {
        log_error("alloc_pages_in_order: 无效的页数 2^%d", order);
        return nullptr;
    }

    return pmm_fetch_free_memblock(order);
}

int pages2order(int pagecnt) {
    switch (pagecnt) {
        case 1:  return 0;
        case 2:  return 1;
        case 3:
        case 4:  return 2;
        default: {
            int order = 3;
            while (order <= MAX_BUDDY_ORDER) {
                if ((1ul << order) >= pagecnt) {
                    break;
                }
                order++;
            }
            return order;
        }
    }
}

void *alloc_pages(int pagecnt) {
    if (pagecnt <= 0 || (pagecnt >> MAX_BUDDY_ORDER) > 1) {
        log_error("alloc_pages: 无效的页数 %d", pagecnt);
        return nullptr;
    }

    // 计算所需的最小order
    int order = pages2order(pagecnt);

    // 分配的页地址
    void *addr = pmm_fetch_free_memblock(order);

    // 如果页为空
    if (addr == nullptr) {
        return nullptr;
    }

    // 将多余的页送还
    pmm_add_free_pages((void *)((umb_t)addr + (pagecnt << 12)),
                       (1ul << order) - pagecnt);
    return addr;
}

void free_page(void *paddr) {
    if (paddr >= (void *)KPHY_VA_OFFSET) {
        log_error("free_page: 地址 %p 超出物理地址范围", paddr);
        return;
    }
    if (((umb_t)paddr % 0x1000) != 0) {
        log_error("free_page: 地址 %p 非页对齐", paddr);
        return;
    }
    pmm_add_free_memblock(paddr, 0);
}

void free_pages_in_order(void *paddr, int order) {
    if (paddr >= (void *)KPHY_VA_OFFSET) {
        log_error("free_page: 地址 %p 超出物理地址范围", paddr);
        return;
    }
    if (((umb_t)paddr % 0x1000) != 0) {
        log_error("free_pages_in_order: 地址 %p 非页对齐", paddr);
        return;
    }
    if (order < 0) {
        log_error("free_pages_in_order: 无效的页数 2^%d", order);
        return;
    }
    if (order > MAX_BUDDY_ORDER) {
        int multiplier = 1 << (order - MAX_BUDDY_ORDER);
        // 先释放多个最大阶数内存块
        for (int i = 0; i < multiplier; i++) {
            pmm_add_free_memblock(
                (void *)((umb_t)paddr + (i * (1ul << (MAX_BUDDY_ORDER + 12)))),
                MAX_BUDDY_ORDER);
        }
        return;
    }
    pmm_add_free_memblock(paddr, order);
}

void free_pages(void *paddr, int pagecnt) {
    if (paddr >= (void *)KPHY_VA_OFFSET) {
        log_error("free_page: 地址 %p 超出物理地址范围", paddr);
        return;
    }
    if (((umb_t)paddr % 0x1000) != 0) {
        log_error("free_pages: 地址 %p 非页对齐", paddr);
        return;
    }
    if (pagecnt <= 0) {
        log_error("free_pages: 无效的页数 %d", pagecnt);
        return;
    }
    pmm_add_free_pages(paddr, pagecnt);
}

void pmm_init(MemRegion *layout) {
    // STEP1: 根据layout解析物理内存布局

    // 初始化FARL
    for (int i = 0; i <= MAX_BUDDY_ORDER; i++) {
        init_farl(i);
    }

    // 加入所有可用的内存区域
    MemRegion *region = layout;
    while (region != nullptr) {
        if (region->status == MEM_REGION_FREE) {
            // 可用内存区域(4K对齐)
            umb_t start_addr = (umb_t)region->addr & ~(0xFFF);
            // 可用页数
            // 其中, 要考虑对齐后损失的页面数
            // 其实质为:
            // ((region->size + region->addr) - start_addr) / PAGE_SIZE
            // 即(region->end_addr - start_addr) / PAGE_SIZE
            umb_t pages =
                (region->size - (start_addr - (umb_t)region->addr)) / PAGE_SIZE;

            // 将其加入物理内存管理器
            pmm_add_free_pages((void *)start_addr, pages);
        }
        // 处理下一个内存区域
        region = region->next;
    }
}

void pmm_post_init(void) {
    // 遍历所有FARL链表
    for (int order = 0; order <= MAX_BUDDY_ORDER; order++) {
        if (free_area_head[order] == nullptr) {
            continue;
        }
        // 将表头迁移到高地址
        // 由于这部分是在stage1分配器下分配的, 因此应迁移到Kernel处而非Kheap处
        // 更新链表中所有节点的地址
        log_debug("pmm_post_init: 迁移PMMFreeArea链表 阶数=%d", order);
        free_area_head[order] = PA2KA(free_area_head[order]);
        free_area_tail[order] = PA2KA(free_area_tail[order]);
        PMMFreeArea *iter     = free_area_head[order];
        while (iter != nullptr) {
            // 更新prev和next指针
            if (iter->prev != nullptr) {
                iter->prev = PA2KA(iter->prev);
            }
            if (iter->next != nullptr) {
                iter->next = PA2KA(iter->next);
            }
            // 链表遍历操作, 例行公事
            iter = iter->next;
        }
    }
}