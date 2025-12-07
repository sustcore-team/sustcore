/**
 * @file alloc.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内存分配器实现
 * @version alpha-1.0.0
 * @date 2025-11-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <alloc.h>
#include <basec/logger.h>
#include <sus/attributes.h>
#include <sus/bits.h>

#define PAGE_SIZE (4096)

static void *heap_start = nullptr, *heap_end = nullptr;

// 从堆中分配页
void *alloc_pages(size_t num_pages) {
    void *addr  = heap_end;
    heap_end   += num_pages * PAGE_SIZE;
    return addr;
}

// 从堆中释放页
void free_pages(void *addr, size_t num_pages) {
    // 不需要释放
}

/**
 * @brief 堆页索引, 指向被分配为内核堆的页
 *
 */
typedef struct __HeapPageIdx__ {
    /**
     * @brief 位图页
     *
     * 位图页中共有2^12byte=2^15bits,
     * 每个位对应着heap_pages中的一个dword的使用情况
     * 因此一个位图页可以控制32个页(order=5)
     *
     */
    byte *bitmap_page;
    /* 堆所在页 */
    void *heap_pages;
    /* 下一个位图页索引 */
    struct __HeapPageIdx__ *next;
    /* 上一个位图页索引 */
    struct __HeapPageIdx__ *last;
} HeapPageIdx;

/**
 * @brief 堆页索引链表头尾
 *
 */
static HeapPageIdx *heap_idx_head, *heap_idx_tail;

/**
 * @brief 向链表加入索引
 *
 * @param idx 索引
 */
static void add_heap_page_idx(HeapPageIdx *idx) {
    // 优先加到表头
    // (局域性原理)
    heap_idx_head->last = idx;
    idx->last           = nullptr;
    idx->next           = heap_idx_head;
    heap_idx_head       = idx;
}

/**
 * @brief 堆位置
 *
 */
typedef struct {
    // 所在堆索引
    HeapPageIdx *idx;
    // 在堆页中的偏移
    size_t offset;
} HeapLocation;

/**
 * @brief 位图中的位置
 *
 */
typedef struct {
    // 在位图的第几个字节
    size_t byte_in_bitmap;
    // 在位图的第几个位
    size_t bit_in_byte;
} InBitmapLocation;

/**
 * @brief 位图中空闲的dword数量
 *
 */
static size_t heap_free_dwords = 0;

#define LEAST_FREE_DWORDS (256)  // 保持至少256个空闲dword

/**
 * @brief 分配一个堆页索引
 *
 */
static bool alloc_heap_page() {
    printf("alloc_heap_page: 分配新的堆页");

    // 首先分配32个页作为堆页
    void *new_heap_pages = alloc_pages(32);

    // 再分配一个页来管理这些页里的pages
    void *new_bitmap_page = alloc_pages(1);

    // 组成一个HeapPage加入结构体
    HeapPageIdx *new_idx = (HeapPageIdx *)malloc(sizeof(HeapPageIdx));
    new_idx->bitmap_page = new_bitmap_page;
    new_idx->heap_pages  = new_heap_pages;

    // 32 pages * 4096 bytes/page = 131072 bytes = 32768 dwords
    heap_free_dwords += 32768;

    add_heap_page_idx(new_idx);
    return true;
}

/**
 * @brief 计算位图中的位置
 *
 * @param location 在堆内的位置
 * @return InBitmapLocation 位图中的位置
 */
static InBitmapLocation locate_in_bitmap(HeapLocation location) {
    InBitmapLocation loc;
    // 计算是第几个dword
    size_t bit_pos     = location.offset / 4;
    // 计算是位图第几个byte的第几个bit
    loc.byte_in_bitmap = bit_pos / 8;
    loc.bit_in_byte    = bit_pos % 8;
    return loc;
}

/**
 * @brief 标记[start, end]的dword为已使用
 *
 * @param start 开始位置
 * @param end 结束位置
 */
static void mark_dirty(HeapLocation start, HeapLocation end) {
    if (start.idx != end.idx) {
        log_error("mark_dirty: 跨页标记不支持");
        return;
    }

    if (start.offset > end.offset) {
        log_error("mark_dirty: 起始位置大于结束位置");
        return;
    }

    if (end.offset / 32 >= 4096) {
        log_error("mark_dirty: offset 超出范围");
        return;
    }

    if ((start.offset & 0x3) != 0 || (end.offset & 0x3) != 0) {
        log_error("mark_dirty: offset 非dword对齐");
        return;
    }

    InBitmapLocation start_loc = locate_in_bitmap(start);
    InBitmapLocation end_loc   = locate_in_bitmap(end);

    // 标记start_loc到end_loc范围内的位为已使用
    // 首先标记加载它们之间的完整byte
    for (size_t byte_idx = start_loc.byte_in_bitmap + 1;
         byte_idx < end_loc.byte_in_bitmap; byte_idx++)
    {
        start.idx->bitmap_page[byte_idx] = 0xFF;
    }

    // 如果start_loc和end_loc在同一个byte中
    if (start_loc.byte_in_bitmap == end_loc.byte_in_bitmap) {
        // 则只需要标记start_loc到end_loc之间的位
        for (size_t bit_idx = start_loc.bit_in_byte;
             bit_idx <= end_loc.bit_in_byte; bit_idx++)
        {
            start.idx->bitmap_page[start_loc.byte_in_bitmap] |= (1 << bit_idx);
        }
    } else {
        // 否则, 标记start_loc所在byte的后续位
        for (size_t bit_idx = start_loc.bit_in_byte; bit_idx < 8; bit_idx++) {
            start.idx->bitmap_page[start_loc.byte_in_bitmap] |= (1 << bit_idx);
        }
        // 并标记end_loc所在byte的前续位
        for (size_t bit_idx = 0; bit_idx <= end_loc.bit_in_byte; bit_idx++) {
            start.idx->bitmap_page[end_loc.byte_in_bitmap] |= (1 << bit_idx);
        }
    }
}

/**
 * @brief 标记[start, end]的dword为未使用
 *
 * @param start 开始位置
 * @param end 结束位置
 */
static void unmark_dirty(HeapLocation start, HeapLocation end) {
    if (start.idx != end.idx) {
        log_error("unmark_dirty: 跨页标记不支持");
        return;
    }

    if (start.offset > end.offset) {
        log_error("unmark_dirty: 起始位置大于结束位置");
        return;
    }

    if (end.offset / 32 >= 4096) {
        log_error("unmark_dirty: offset 超出范围");
        return;
    }

    if ((start.offset & 0x3) != 0 || (end.offset & 0x3) != 0) {
        log_error("unmark_dirty: offset 非dword对齐");
        return;
    }

    InBitmapLocation start_loc = locate_in_bitmap(start);
    InBitmapLocation end_loc   = locate_in_bitmap(end);

    // 标记start_loc到end_loc范围内的位为未使用
    // 首先标记加载它们之间的完整byte
    for (size_t byte_idx = start_loc.byte_in_bitmap + 1;
         byte_idx < end_loc.byte_in_bitmap; byte_idx++)
    {
        start.idx->bitmap_page[byte_idx] = 0x00;
    }

    // 如果start_loc和end_loc在同一个byte中
    if (start_loc.byte_in_bitmap == end_loc.byte_in_bitmap) {
        // 则只需要标记start_loc到end_loc之间的位
        for (size_t bit_idx = start_loc.bit_in_byte;
             bit_idx <= end_loc.bit_in_byte; bit_idx++)
        {
            start.idx->bitmap_page[start_loc.byte_in_bitmap] &= ~(1 << bit_idx);
        }
    } else {
        // 否则, 标记start_loc所在byte的后续位
        for (size_t bit_idx = start_loc.bit_in_byte; bit_idx < 8; bit_idx++) {
            start.idx->bitmap_page[start_loc.byte_in_bitmap] &= ~(1 << bit_idx);
        }
        // 并标记end_loc所在byte的前续位
        for (size_t bit_idx = 0; bit_idx <= end_loc.bit_in_byte; bit_idx++) {
            start.idx->bitmap_page[end_loc.byte_in_bitmap] &= ~(1 << bit_idx);
        }
    }
}

/**
 * @brief 寻找堆页中空闲的dword位置
 *
 * @param needed_words 需要的连续空闲dword数量
 * @param page_idx 堆页索引
 * @return HeapLocation 空闲位置
 */
static HeapLocation search_free_dwords_in_page(size_t needed_words,
                                               HeapPageIdx *page_idx) {
    // 连续空闲dword数量
    size_t consecutive_free = 0;
    HeapLocation location;
    location.idx    = page_idx;
    location.offset = 0;

    // 遍历位图中所有的字节
    for (size_t byte_idx = 0; byte_idx < 4096; byte_idx++) {
        // 位图中
        byte bitmap_byte = page_idx->bitmap_page[byte_idx];
        if (bitmap_byte == 0xFF) {
            // 全部已使用
            consecutive_free = 0;
            continue;
        }
        if (bitmap_byte == 0x00) {
            // 全部空闲
            if (consecutive_free == 0) {
                // 记录起始位置
                location.offset = byte_idx * 8 * 4;
            }
            consecutive_free += 8;
            if (consecutive_free >= needed_words) {
                // 找到足够的连续空闲dword
                return location;
            }
            continue;
        }

        // 遍历其中的位
        for (size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            bool is_used = (bitmap_byte & (1 << bit_idx)) != 0;
            if (!is_used) {
                // 空闲
                if (consecutive_free == 0) {
                    // 记录起始位置
                    location.offset = (byte_idx * 8 + bit_idx) * 4;
                }
                consecutive_free++;
                if (consecutive_free >= needed_words) {
                    // 找到足够的连续空闲dword
                    return location;
                }
            } else {
                // 已使用, 重置计数器
                consecutive_free = 0;
            }
        }
    }

    // 未找到合适的位置
    location.idx    = nullptr;
    location.offset = 0;
    return location;
}

/**
 * @brief 寻找位图中空闲的dword
 *
 * @param need_dwords 需要的dword数量
 * @return HeapLocation 空闲位置
 */
static HeapLocation search_free_dwords(size_t need_dwords) {
    // 遍历所有堆页索引
    HeapPageIdx *idx = heap_idx_head;
    while (idx != nullptr) {
        HeapLocation loc = search_free_dwords_in_page(need_dwords, idx);
        if (loc.idx != nullptr) {
            // 找到合适的位置
            return loc;
        }
        idx = idx->next;
    }

    // 未找到合适的位置
    HeapLocation location;
    location.idx    = nullptr;
    location.offset = 0;
    return location;
}

/**
 * @brief 分配信息
 *
 */
typedef struct __AllocInfo__ {
    // 是否为page
    bool is_page;
    // 是否被用过
    bool is_used;
    // 起始位置
    HeapLocation start;
    // 结束位置
    HeapLocation end;
    // 起始页地址
    void *start_page;
    // 页数目
    int page_count;
    // 上个分配信息条目
    struct __AllocInfo__ *last;
    // 下个分配信息条目
    struct __AllocInfo__ *next;
} AllocInfo;

/**
 * @brief 分配信息链表头
 *
 */
static AllocInfo *alloc_info_head = nullptr;

/**
 * @brief 空闲的分配信息链表头
 *
 */
static AllocInfo *empty_alloc_info_head = nullptr;

/**
 * @brief 剩余可用的分配信息数量
 *
 */
static int empty_alloc_info_count = 0;

/**
 * @brief 是否正在扩展空闲分配信息
 *
 */
static bool extending_empty_alloc_infos = false;

/**
 * @brief 添加空闲分配信息
 *
 * @param extend_count 拓展数量
 * @param addr 分配信息地址
 */
static void __extend_empty_alloc_infos(const int extend_count, void *addr) {
    // 说明正在拓展状态
    extending_empty_alloc_infos = true;
    // 分配内存用于新的分配信息
    AllocInfo *info             = (AllocInfo *)addr;

    // 初始化这些分配信息
    for (int i = 0; i < extend_count; i++) {
        info[i].is_used = false;
    }

    // 将它们加入空闲链表
    // extend_count - 1 -> ... -> 1 -> 0 -> empty_alloc_info_head

    // 头
    info[0].next = empty_alloc_info_head;
    if (empty_alloc_info_head != nullptr) {
        empty_alloc_info_head->last = &info[0];
    }
    info[0].last = &info[1];

    // 中间部分
    for (int i = 1; i < extend_count - 1; i++) {
        info[i].next = &info[i - 1];
        info[i].last = nullptr;
    }
    // 尾巴
    info[extend_count - 1].next = &info[extend_count - 2];
    info[extend_count - 1].last = nullptr;
    empty_alloc_info_head       = &info[extend_count - 1];

    // 退出拓展状态
    extending_empty_alloc_infos  = false;
    empty_alloc_info_count      += extend_count;
}

/**
 * @brief 添加空闲分配信息
 *
 * 在空闲信息用完前就应该使用这个
 *
 * @param info 空闲分配信息
 */
static void extend_empty_alloc_infos(const int extend_count) {
    __extend_empty_alloc_infos(extend_count,
                               malloc(sizeof(AllocInfo) * extend_count));
}

/**
 * @brief 获得空闲分配信息
 *
 * @return AllocInfo* 分配信息指针
 */
static AllocInfo *get_empty_alloc_info(void) {
    if (empty_alloc_info_head == nullptr) {
        log_error("get_empty_alloc_info: 分配信息链表为空");
        return nullptr;
    }

    // 如果空闲分配信息不足, 且不在扩展状态中, 则扩展
    if (!extending_empty_alloc_infos && empty_alloc_info_count < 16) {
        extend_empty_alloc_infos(512);
    }

    // 将链表头取出
    AllocInfo *info = empty_alloc_info_head;

    empty_alloc_info_head = info->next;
    if (empty_alloc_info_head != nullptr) {
        empty_alloc_info_head->last = nullptr;
    }
    empty_alloc_info_count--;

    return info;
}

/**
 * @brief 添加分配信息到使用列表
 *
 * @param info 分配信息指针
 */
static void add_alloc_info(AllocInfo *info) {
    info->is_used = true;

    // 如果无头, 设置为头
    if (alloc_info_head == nullptr) {
        alloc_info_head = info;
        info->last      = nullptr;
        info->next      = nullptr;
        return;
    }

    // 添加到列表头
    alloc_info_head->last = info;
    info->next            = alloc_info_head;
    info->last            = nullptr;
    alloc_info_head       = info;
}

/**
 * @brief 添加小块内存分配信息
 *
 * @param start 起始位置
 * @param end 结束位置
 */
static void add_small_alloc_info(HeapLocation start, HeapLocation end) {
    AllocInfo *info = get_empty_alloc_info();
    if (info == nullptr) {
        log_error("add_small_alloc_info: 无可用的分配信息");
        return;
    }

    // 填充信息
    info->is_page = false;
    info->start   = start;
    info->end     = end;
    add_alloc_info(info);
}

/**
 * @brief 添加页分配信息
 *
 * @param start_page 起始页
 * @param page_count 页数量
 */
static void add_page_alloc_info(void *start_page, int page_count) {
    AllocInfo *info = get_empty_alloc_info();
    if (info == nullptr) {
        log_error("add_page_alloc_info: 无可用的分配信息");
        return;
    }

    // 填充信息
    info->is_page    = true;
    info->start_page = start_page;
    info->page_count = page_count;
    add_alloc_info(info);
}

/**
 * @brief 释放分配信息
 *
 * @param info 分配信息指针
 */
static void release_page_alloc_info(AllocInfo *info) {
    if (!info->is_used) {
        log_error("release_page_alloc_info: 分配信息未被使用");
        return;
    }
    // 将其标记为未使用
    info->is_used   = false;
    AllocInfo *prev = info->last;
    // 移出使用列表
    if (prev == nullptr) {
        // info -> next => next
        alloc_info_head = info->next;
        if (alloc_info_head != nullptr) {
            alloc_info_head->last = nullptr;
        }
    } else {
        // prev -> info -> next => prev -> next
        prev->next = info->next;
        if (info->next != nullptr) {
            info->next->last = prev;
        }
    }
    // 加入空闲链表
    info->next = empty_alloc_info_head;
    info->last = nullptr;
    if (empty_alloc_info_head != nullptr) {
        empty_alloc_info_head->last = info;
    }
    empty_alloc_info_head = info;
    empty_alloc_info_count++;
}

// 匹配分配信息
static AllocInfo *match_alloc_info(void *addr) {
    // 遍历分配信息链表
    for (AllocInfo *info = alloc_info_head; info != nullptr; info = info->next)
    {
        if (info->is_page) {
            // 大页分配
            if (info->start_page == addr) {
                return info;
            }
        } else {
            // 小块分配
            void *start_addr = (void *)((umb_t)info->start.idx->heap_pages +
                                        info->start.offset);
            if (addr == start_addr) {
                return info;
            }
        }
    }
    return nullptr;
}

// 分配内存
void *malloc(size_t size) {
    if (size >= 4096) {
        // 将其向上对齐至page, 并分配page
        size_t needed_pages = (size + 4095) / 4096;
        void *addr          = alloc_pages(needed_pages);
        if (addr == nullptr) {
            log_error("malloc: 大页分配失败 size=%u", size);
            return nullptr;
        }
        // 将其记录在分配记录中
        add_page_alloc_info(addr, needed_pages);
        return addr;
    }

    if (heap_idx_head == nullptr) {
        log_error("malloc: 堆页索引未初始化");
        return nullptr;
    }

    // 计算所需dword数
    size_t needed_dwords = (size + 3) / 4;

    // 如果当前空闲dword数量不足, 则分配新的堆页
    while (heap_free_dwords < needed_dwords + LEAST_FREE_DWORDS) {
        if (!alloc_heap_page()) {
            log_error("malloc: 无法分配新的堆页");
            return nullptr;
        }
    }

    // 寻找空闲dword位置
    HeapLocation loc = search_free_dwords(needed_dwords);
    if (loc.idx == nullptr) {
        // 堆内存不足, 分配一个新堆页
        if (!alloc_heap_page()) {
            log_error("malloc: 无法分配新的堆页");
            return nullptr;
        }
        loc = search_free_dwords(needed_dwords);
        if (loc.idx == nullptr) {
            log_error("malloc: 分配新堆页后仍无法找到空闲位置");
            return nullptr;
        }
    }

    // 标记为已使用
    HeapLocation end_loc;
    end_loc.idx    = loc.idx;
    end_loc.offset = loc.offset + needed_dwords * 4 - 4;
    mark_dirty(loc, end_loc);

    // 更新空闲dword数量
    heap_free_dwords -= needed_dwords;

    if (heap_free_dwords < 20) {
        // 及时分配新的堆页
        if (!alloc_heap_page()) {
            log_error("malloc: 无法分配新的堆页");
            return nullptr;
        }
    }

    // 将其记录在分配记录中
    add_small_alloc_info(loc, end_loc);

    // 计算实际地址
    void *addr = (void *)((umb_t)loc.idx->heap_pages + loc.offset);

    return addr;
}

// 释放内存
void free(void *ptr) {
    AllocInfo *info = match_alloc_info(ptr);
    if (info == nullptr) {
        return;
    }
    if (info->is_page) {
        // 大页分配
        free_pages(info->start_page, info->page_count);
    } else {
        // 小块分配
        // 标记为未使用
        unmark_dirty(info->start, info->end);
        // 更新空闲dword数量
        size_t freed_dwords  = (info->end.offset - info->start.offset) / 4 + 1;
        heap_free_dwords    += freed_dwords;
    }
    // 释放分配信息
    release_page_alloc_info(info);
}

/**
 * @brief 初始化内存分配器
 *
 * @param heap_ptr 堆指针
 */
void init_malloc(void *heap_ptr) {
    // 初始化堆头和堆尾指针
    heap_start = heap_end = heap_ptr;

    // 将前面的一个页(除去第一个sizeof(HeapPageIdx)字节)留作分配信息使用
    void *initial_page = alloc_pages(1);

    // 分配初始堆页
    void *bitmap_page        = alloc_pages(1);
    void *heap_pages         = alloc_pages(32);
    // 作为heap_page_idx使用
    HeapPageIdx *initial_idx = (HeapPageIdx *)initial_page;
    initial_idx->bitmap_page = bitmap_page;
    initial_idx->heap_pages  = heap_pages;
    // 增加free dwords
    heap_free_dwords += 32768;

    // 加入到链表中
    initial_idx->next = nullptr;
    initial_idx->last = nullptr;
    heap_idx_head = heap_idx_tail = initial_idx;

    // 剩余的空间用来存放分配信息
    void *first_alloc_info_addr = (void *)(initial_page + sizeof(HeapPageIdx));
    __extend_empty_alloc_infos(
        (PAGE_SIZE - sizeof(HeapPageIdx)) / sizeof(AllocInfo),
        first_alloc_info_addr);
}