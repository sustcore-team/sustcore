/**
 * @file mem_cap.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内存能力
 * @version alpha-1.0.0
 * @date 2026-01-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/mem_cap.h>
#include <mem/alloc.h>
#include <mem/buddy.h>

CapIdx mem_cap_create(PCB *p, void *paddr, size_t size, bool shared, bool mmio, bool allocated)
{
    MemoryData *mem_data = (MemoryData *)kmalloc(sizeof(MemoryData));
    if (mem_data == nullptr) {
        return INVALID_CAP_IDX;
    }

    mem_data->mem_paddr = paddr;
    mem_data->mem_size = size;
    mem_data->shared = shared;
    mem_data->mmio = mmio;
    mem_data->allocated = allocated;

    CapIdx ptr = create_cap(p, CAP_TYPE_MEM, (void *)mem_data, CAP_PRIV_ALL, nullptr);
    if (CAPIDX_INVALID(ptr)) {
        // 创建失败, 释放内存
        kfree(mem_data);
        return INVALID_CAP_IDX;
    }
    return ptr;
}

CapIdx mem_cap_alloc_and_create(PCB *p, size_t size, bool shared)
{
    void *paddr = alloc_pages(SIZE2PAGES(size));
    if (paddr == nullptr) {
        return INVALID_CAP_IDX;
    }
    CapIdx ptr = mem_cap_create(p, paddr, size, shared, false, true);
    if (CAPIDX_INVALID(ptr)) {
        free_pages(paddr, SIZE2PAGES(size));
        return INVALID_CAP_IDX;
    }
    return ptr;
}

