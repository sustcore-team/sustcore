/**
 * @file vmm.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 虚拟内存管理
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <mem/alloc.h>
#include <mem/kmem.h>
#include <mem/pmm.h>
#include <mem/vmm.h>
#include <string.h>
#include <sus/paging.h>


#ifdef DLOG_PMM
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

void *setup_paging_table(void) {
    // 构造根页表
    void *root = mem_construct_root();
    // 加载内核分页
    create_kernel_paging(root);
    return root;
}

void alloc_pages_for(void *root, void *vaddr, size_t pages, int rwx,
                     bool user) {
    // 我们希望分配的页尽可能连续
    // 我们可以做出如下处理

    // 1. 计算需要的最小order
    int order = pages2order(pages);

    void *paddr      = nullptr;
    void *cur_vaddr  = vaddr;
    int remain_pages = pages;

    // 2. 尽可能分配大块的页
    //    直到所有页分配完
    while (remain_pages > 0) {
        // 尝试分配order阶的页
        while (order > 0 && paddr == nullptr) {
            // 如果 (1 << order) > (remain_pages), 我们就尝试先alloc_pages
            if (remain_pages >> order == 0) {
                // 尝试分配剩余页数
                paddr = alloc_pages(remain_pages);
            } else {
                // 否则尝试分配2^order页
                paddr = alloc_pages_in_order(order);
            }
            // paddr为nullptr时持续降低
            if (paddr == nullptr)
                order--;
        }

        if (paddr == nullptr) {
            // 此时就连order = 0的页都无法分配
            // 也就是说连一个页都分配不了
            // 这个时候还要什么自行车啊, 内存都被用尽了.
            log_error("alloc_pages_for: 无法分配任何页 vaddr=%p pages=%d",
                      vaddr, pages);
            // TODO: 回收已分配的页
            return;
        }

        size_t block_pages = 1ul << (order + 1);
        if (block_pages > remain_pages) {
            block_pages = remain_pages;
        }
        // 分配成功, 映射该块
        mem_maps_range_to(root, cur_vaddr, paddr, block_pages, rwx, user, true);

        // 重置paddr以便下一次分配
        paddr         = nullptr;
        // 更新cur_vaddr与remain_pages
        cur_vaddr     = (void *)((umb_t)cur_vaddr + (block_pages << 12));
        remain_pages -= block_pages;
    }
}

void memcpy_u2u(void *dest_root, void *dest_vaddr, void *src_root,
                void *src_vaddr, size_t size) {
    // 临时缓冲区
    void *temp_buf = kmalloc(size);
    if (temp_buf == nullptr) {
        log_error("memcpy_u2u: 分配临时缓冲区失败 size=%lu", size);
        return;
    }

    // 从源用户空间复制到临时缓冲区
    memcpy_u2k(src_root, temp_buf, src_vaddr, size);
    // 从临时缓冲区复制到目标用户空间
    memcpy_k2u(dest_root, dest_vaddr, temp_buf, size);

    // 释放临时缓冲区
    kfree(temp_buf);
}

void memcpy_k2u(void *root, void *dest_vaddr, void *src_kaddr, size_t size) {
    // 从内核空间复制到用户空间
    // 我们从页表出发逐页复制
    void *cur_dest_vaddr = dest_vaddr;
    void *cur_src_kaddr  = src_kaddr;
    int remain_size      = size;

    while (remain_size > 0) {
        // 获得cur_dest_vaddr所在页表项
        void *aligned_dest_vaddr =
            (void *)((umb_t)cur_dest_vaddr & ~(PAGE_SIZE - 1));
        LargablePTEntry entry = mem_get_page(root, aligned_dest_vaddr);

        // 得到对应的内核物理地址与页大小
        void *kpaddr  = PA2KPA(mem_pte_dst(entry.entry));
        size_t pagesz = PAGE_SIZE_BY_LEVEL(entry.level);

        // 计算在该页内的偏移
        size_t offset = (umb_t)cur_dest_vaddr - (umb_t)aligned_dest_vaddr;

        // 计算本页可复制的大小
        size_t copysz = pagesz - offset;
        if (copysz > remain_size) {
            copysz = remain_size;
        }

        // 于是我们将数据复制到该页
        memcpy((void *)((umb_t)kpaddr + offset), cur_src_kaddr, copysz);

        // 更新指针和剩余大小
        cur_dest_vaddr  = (void *)((umb_t)cur_dest_vaddr + copysz);
        cur_src_kaddr  += copysz;
        remain_size    -= copysz;
    }
}

void memcpy_u2k(void *root, void *dest_kaddr, void *src_vaddr, size_t size) {
    // 这个与k2u是类似的, 只是反过来

    void *cur_dest_kaddr = dest_kaddr;
    void *cur_src_vaddr  = src_vaddr;
    int remain_size      = size;

    while (remain_size > 0) {
        // 获得cur_src_vaddr所在页表项
        void *aligned_src_vaddr =
            (void *)((umb_t)cur_src_vaddr & ~(PAGE_SIZE - 1));
        LargablePTEntry entry = mem_get_page(root, aligned_src_vaddr);

        // 得到对应的内核物理地址与页大小
        void *kpaddr  = PA2KPA(mem_pte_dst(entry.entry));
        size_t pagesz = PAGE_SIZE_BY_LEVEL(entry.level);

        // 计算在该页内的偏移
        size_t offset = (umb_t)cur_src_vaddr - (umb_t)aligned_src_vaddr;

        // 计算本页可复制的大小
        size_t copysz = pagesz - offset;
        if (copysz > remain_size) {
            copysz = remain_size;
        }

        // 于是我们将数据复制到该页
        memcpy(cur_dest_kaddr, (void *)((umb_t)kpaddr + offset), copysz);

        // 更新指针和剩余大小
        cur_dest_kaddr  = (void *)((umb_t)cur_dest_kaddr + copysz);
        cur_src_vaddr   = (void *)((umb_t)cur_src_vaddr + copysz);
        remain_size    -= copysz;
    }
}

void memset_u(void *root, void *vaddr, int byte, size_t size) {
    // 逐页设置
    void *cur_vaddr = vaddr;
    int remain_size = size;

    while (remain_size > 0) {
        // 获得cur_vaddr所在页表项
        void *aligned_vaddr   = (void *)((umb_t)cur_vaddr & ~(PAGE_SIZE - 1));
        LargablePTEntry entry = mem_get_page(root, aligned_vaddr);

        // 得到对应的内核物理地址与页大小
        void *kpaddr  = PA2KPA(mem_pte_dst(entry.entry));
        size_t pagesz = PAGE_SIZE_BY_LEVEL(entry.level);

        // 计算在该页内的偏移
        size_t offset = (umb_t)cur_vaddr - (umb_t)aligned_vaddr;

        // 计算本页可设置的大小
        size_t setsz = pagesz - offset;
        if (setsz > remain_size) {
            setsz = remain_size;
        }

        // 于是我们将数据设置到该页
        memset((void *)((umb_t)kpaddr + offset), byte, setsz);

        // 更新指针和剩余大小
        cur_vaddr    = (void *)((umb_t)cur_vaddr + setsz);
        remain_size -= setsz;
    }
}

/**
 * @brief 用户空间内存比较
 * 
 * @param root1 页表根指针1
 * @param vaddr1 虚拟地址1
 * @param root2 页表根指针2
 * @param vaddr2 虚拟地址2
 * @param size 大小
 * @return int 比较结果
 */
int memcmp_u2u(void *root1, void *vaddr1, void *root2, void *vaddr2, size_t size)
{
    // 设置一个临时缓冲区
    void *temp_buf1 = kmalloc(size);
    if (temp_buf1 == nullptr) {
        log_error("memcmp_u2u: 分配临时缓冲区1失败 size=%lu", size);
        return -1;
    }
    void *temp_buf2 = kmalloc(size);
    if (temp_buf2 == nullptr) {
        log_error("memcmp_u2u: 分配临时缓冲区2失败 size=%lu", size);
        kfree(temp_buf1);
        return -1;
    }

    // 从用户空间复制到临时缓冲区
    memcpy_u2k(root1, temp_buf1, vaddr1, size);
    memcpy_u2k(root2, temp_buf2, vaddr2, size);

    // 进行比较
    int result = memcmp(temp_buf1, temp_buf2, size);

    // 释放临时缓冲区
    kfree(temp_buf1);
    kfree(temp_buf2);

    return result;
}

void ua_start_access(void) {
    csr_sstatus_t sstatus = csr_get_sstatus();
    sstatus.sum = 1;  // 允许S-MODE访问U-MODE内存
    csr_set_sstatus(sstatus);
}

void ua_end_access(void) {
    csr_sstatus_t sstatus = csr_get_sstatus();
    sstatus.sum = 0;  // 禁止S-MODE访问U-MODE内存
    csr_set_sstatus(sstatus);
}