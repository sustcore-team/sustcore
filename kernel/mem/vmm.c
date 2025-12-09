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
#include <sus/list_helper.h>


#ifdef DLOG_PMM
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

TM *setup_task_memory(void) {
    TM *tm = (TM *)kmalloc(sizeof(TM));
    // 初始化VMA链表
    ordered_list_init(TM_VMA_LIST(tm));
    tm->pgd           = mem_construct_root();
    // 加载内核分页
    create_kernel_paging(tm->pgd);
    return tm;
}

void add_vma(TM *tm, void *vaddr, size_t size, VMAType type) {
    VMA *vma = (VMA *)kmalloc(sizeof(VMA));
    if (vma == nullptr) {
        log_error("add_vma: 分配VMA结构体失败 vaddr=%p size=%lu type=%d",
                  vaddr, size, type);
        return;
    }
    vma->vaddr = vaddr;
    vma->size  = size;
    vma->type  = type;

    // 插入有序链表
    ordered_list_insert(vma, TM_VMA_LIST(tm));
}

void remove_vma(TM *tm, void *vaddr) {
    VMA *vma = nullptr;
    // 查找对应VMA
    ordered_list_findeq(vaddr, vma, equal_to, TM_VMA_LIST(tm));
    if (vma == nullptr) {
        log_error("remove_vma: 未找到对应VMA vaddr=%p", vaddr);
        return;
    }
    // 从链表中移除并释放
    ordered_list_remove(vma, TM_VMA_LIST(tm));
    kfree(vma);
}

VMA *find_vma(TM *tm, void *vaddr) {
    VMA *vma = nullptr;

    // 遍历链表查找vaddr所在的VMA
    foreach_ordered_list(vma, TM_VMA_LIST(tm)) {
        // 如果vma->vaddr > vaddr
        // 说明已经越过了可能的VMA
        if (vma->vaddr > vaddr) {
            break;
        }

        // 检查vaddr是否在vma范围内
        if ((vma->vaddr + vma->size) > vaddr) {
            return vma;
        }
    }

    return vma;
}

/**
 * @brief 物理内存区域结构体
 * 
 */
typedef struct PMAStruct {
    // 形成链表结构
    struct PMAStruct *prev;
    struct PMAStruct *next;

    // 物理地址
    void *paddr;
    // 大小
    int pages;
} PMA;

PMA *pma_list_head = nullptr;
PMA *pma_list_tail = nullptr;

#define PMA_LIST \
    pma_list_head, pma_list_tail, prev, next, paddr, ascending

/**
 * @brief 添加PMA
 * 
 * @param paddr 物理地址
 * @param pages 大小(页数)
 */
void add_pma(void *paddr, int pages) {
    PMA *pma = (PMA *)kmalloc(sizeof(PMA));
    if (pma == nullptr) {
        log_error("add_pma: 分配PMA结构体失败 paddr=%p pages=%d", paddr, pages);
        return;
    }

    pma->paddr         = paddr;
    pma->pages         = pages;

    // 插入PMA链表
    ordered_list_insert(pma, PMA_LIST);
}

/**
 * @brief 分配物理页并添加到PMA链表
 * 
 * @param remain_pages 剩余页数
 * @return void* 分配的物理地址
 */
void *pma_alloc_pages(int remain_pages) {
    void *paddr = alloc_pages(remain_pages);
    if (paddr == nullptr) {
        return nullptr;
    }

    add_pma(paddr, remain_pages);
    return paddr;
}

/**
 * @brief 分配order阶物理页并添加到PMA链表
 * 
 * @param order 阶数
 * @return void* 分配的物理地址
 */
void *pma_alloc_pages_in_order(int order) {
    void *paddr = alloc_pages_in_order(order);
    if (paddr == nullptr) {
        return nullptr;
    }

    add_pma(paddr, 1ul << order);
    return paddr;
}

/**
 * @brief 分配单个物理页并添加到PMA链表
 * 
 * @return void* 分配的物理地址
 */
void *pma_alloc_page(void) {
    return pma_alloc_pages_in_order(0);
}

int from_seg_to_rwx(VMAType type)
{
    switch (type) {
        case VMAT_CODE:
            return RWX_MODE_RX;
        case VMAT_DATA:
        case VMAT_STACK:
        case VMAT_HEAP:
        case VMAT_MMAP:
        case VMAT_SHARE_RW:
            return RWX_MODE_RW;
        case VMAT_SHARE_RO:
            return RWX_MODE_R;
        case VMAT_SHARE_RX:
            return RWX_MODE_RX;
        case VMAT_SHARE_RWX:
            return RWX_MODE_RWX;
        default:
            return 0;
    }
}

bool alloc_pages_for(TM *tm, void *vaddr, size_t pages, int rwx, bool user)
{
    // 首先判断[vaddr, vaddr + pages * PAGE_SIZE)是否在某个VMA内
    VMA *vma = find_vma(tm, vaddr);
    if (vma == nullptr) {
        log_error("alloc_pages_for: 未找到对应VMA vaddr=%p pages=%lu", vaddr,
                  pages);
        return false;
    }
    if ((umb_t)vaddr + pages * PAGE_SIZE >
        (umb_t)vma->vaddr + vma->size)
    {
        log_error(
            "alloc_pages_for: 分配范围超出VMA边界 vaddr=%p pages=%lu vma_vaddr=%p vma_size=%lu",
            vaddr, pages, vma->vaddr, vma->size);
        return false;
    }
    log_info("为vaddr=%p分配%lu页 rwx=%d user=%d", vaddr, pages, rwx, user);

    // 接下来分配物理页并建立映射
    // 我们希望分配的页尽可能连续
    // 我们可以做出如下处理

    // 只分配一个页
    if (pages == 1) {
        void *paddr = pma_alloc_page();
        if (paddr == nullptr) {
            log_error("alloc_pages_for: 无法分配页 vaddr=%p pages=%d", vaddr,
                      pages);
            return false;
        }
        mem_maps_range_to(tm->pgd, vaddr, paddr, 1, rwx, user, true);
        return true;
    }

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
                paddr = pma_alloc_pages(remain_pages);
            } else {
                // 否则尝试分配2^order页
                paddr = pma_alloc_pages_in_order(order);
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
            // TODO: 收回已经分配的页
            return false;
        }

        // 计算本次分配的页数
        size_t block_pages = 1ul << (order + 1);
        if (block_pages > remain_pages) {
            block_pages = remain_pages;
        }
        // 分配成功, 映射该块
        mem_maps_range_to(tm->pgd, cur_vaddr, paddr, block_pages, rwx, user, true);

        // 重置paddr以便下一次分配
        paddr         = nullptr;
        // 更新cur_vaddr与remain_pages
        cur_vaddr     = (void *)((umb_t)cur_vaddr + (block_pages << 12));
        remain_pages -= block_pages;
    }
    return true;
}

bool on_np_page_fault(TM *tm, void *vaddr)
{
    VMA *vma = find_vma(tm, vaddr);
    if (vma == nullptr) {
        log_error("on_np_page_fault: 未找到对应VMA vaddr=%p", vaddr);
        return false;
    }
    // 这说明, 其存在于进程的虚拟地址空间内
    // 但不存在于页表中
    // 我们需要为其分配页
    // 我们暂时先一次分配4个页(16KB)
    size_t page_start = (umb_t)vaddr & ~(PAGE_SIZE - 1);
    bool ret = alloc_pages_for(tm, (void *)page_start, 4, from_seg_to_rwx(vma->type),
                    true);
    if (! ret) {
        log_error("on_np_page_fault: 为vaddr=%p分配页失败", vaddr);
        return false;
    }
    return true;
}

bool on_write_ro_page_fault(TM *tm, void *vaddr)
{
    return false;
}

bool on_write_rx_page_fault(TM *tm, void *vaddr)
{
    return false;
}

bool on_execute_rw_page_fault(TM *tm, void *vaddr)
{
    return false;
}

void memcpy_u2u(TM *dst_tm, void *dst_vaddr, TM *src_tm,
                void *src_vaddr, size_t size) {
    // 临时缓冲区
    void *temp_buf = kmalloc(size);
    if (temp_buf == nullptr) {
        log_error("memcpy_u2u: 分配临时缓冲区失败 size=%lu", size);
        return;
    }

    // 从源用户空间复制到临时缓冲区
    memcpy_u2k(src_tm, temp_buf, src_vaddr, size);
    // 从临时缓冲区复制到目标用户空间
    memcpy_k2u(dst_tm, dst_vaddr, temp_buf, size);

    // 释放临时缓冲区
    kfree(temp_buf);
}

void memcpy_k2u(TM *tm, void *dst_vaddr, void *src_kaddr, size_t size) {
    // 从内核空间复制到用户空间
    // 我们从页表出发逐页复制
    void *cur_dst_vaddr = dst_vaddr;
    void *cur_src_kaddr  = src_kaddr;
    int remain_size      = size;

    while (remain_size > 0) {
        // 获得cur_dst_vaddr所在页表项
        void *aligned_dst_vaddr =
            (void *)((umb_t)cur_dst_vaddr & ~(PAGE_SIZE - 1));
        LargablePTEntry entry = mem_get_page(tm->pgd, aligned_dst_vaddr);

        // 得到对应的内核物理地址与页大小
        void *kpaddr  = PA2KPA(mem_pte_dst(entry.entry));
        size_t pagesz = PAGE_SIZE_BY_LEVEL(entry.level);

        // 计算在该页内的偏移
        size_t offset = (umb_t)cur_dst_vaddr - (umb_t)aligned_dst_vaddr;

        // 计算本页可复制的大小
        size_t copysz = pagesz - offset;
        if (copysz > remain_size) {
            copysz = remain_size;
        }

        // 于是我们将数据复制到该页
        memcpy((void *)((umb_t)kpaddr + offset), cur_src_kaddr, copysz);

        // 更新指针和剩余大小
        cur_dst_vaddr  = (void *)((umb_t)cur_dst_vaddr + copysz);
        cur_src_kaddr  += copysz;
        remain_size    -= copysz;
    }
}

void memcpy_u2k(TM *tm, void *dst_kaddr, void *src_vaddr, size_t size) {
    // 这个与k2u是类似的, 只是反过来

    void *cur_dst_kaddr = dst_kaddr;
    void *cur_src_vaddr  = src_vaddr;
    int remain_size      = size;

    while (remain_size > 0) {
        // 获得cur_src_vaddr所在页表项
        void *aligned_src_vaddr =
            (void *)((umb_t)cur_src_vaddr & ~(PAGE_SIZE - 1));
        LargablePTEntry entry = mem_get_page(tm->pgd, aligned_src_vaddr);

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
        memcpy(cur_dst_kaddr, (void *)((umb_t)kpaddr + offset), copysz);

        // 更新指针和剩余大小
        cur_dst_kaddr  = (void *)((umb_t)cur_dst_kaddr + copysz);
        cur_src_vaddr   = (void *)((umb_t)cur_src_vaddr + copysz);
        remain_size    -= copysz;
    }
}

void memset_u(TM *tm, void *vaddr, int byte, size_t size) {
    // 逐页设置
    void *cur_vaddr = vaddr;
    int remain_size = size;

    while (remain_size > 0) {
        // 获得cur_vaddr所在页表项
        void *aligned_vaddr   = (void *)((umb_t)cur_vaddr & ~(PAGE_SIZE - 1));
        LargablePTEntry entry = mem_get_page(tm->pgd, aligned_vaddr);

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
int memcmp_u2u(TM *tm1, void *vaddr1, TM *tm2, void *vaddr2, size_t size)
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
    memcpy_u2k(tm1, temp_buf1, vaddr1, size);
    memcpy_u2k(tm2, temp_buf2, vaddr2, size);

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