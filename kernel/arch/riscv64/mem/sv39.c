/**
 * @file sv39.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SV39分页机制实现
 * @version alpha-1.0.0
 * @date 2025-11-20
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <arch/riscv64/mem/sv39.h>
#include <basec/logger.h>

static Sv39AllocPageFunc sv39_alloc_page = nullptr;

/**
 * @brief 初始化SV39页表映射机制
 * 
 * @param func 页框分配函数
 */
void sv39_mapping_init(Sv39AllocPageFunc func) {
    sv39_alloc_page = func;
}

void sv39_mapping(SV39PTE *root, void *vaddr, void *paddr, umb_t rwx, bool u, bool g)
{
    if (sv39_alloc_page == nullptr) {
        log_error("sv39_mapping: 未设置页框分配函数");
        return;
    }

    if (rwx == RWX_MODE_P) {
        // 不允许设置为指向下一级页表
        log_error("sv39_mapping: 无效rwx模式RWX_MODE_P");
        return;
    }

    // 将vaddr分解为三级页表索引
    umb_t vpn[3];
    umb_t va = (umb_t)vaddr;
    vpn[0] = (va >> 12) & 0x1FF;
    vpn[1] = (va >> 21) & 0x1FF;
    vpn[2] = (va >> 30) & 0x1FF;

    // 搜索第一级页表
    SV39PTE *pte = &root[vpn[2]];
    // 搜索第二级, 第三级
    for (int level = 2; level > 0; level--) {
        if (!(pte->v) || (pte->rwx != RWX_MODE_P)) {
            // 分配新的页表(等待alloc_page实现)
            SV39PT *new_table = (SV39PT *)sv39_alloc_page();
            // 初始化新的页表项
            pte->value = 0;
            pte->v = 1;
            pte->rwx = RWX_MODE_P;
            pte->ppn = phyaddr2ppn(new_table);
        }
        // 取其中相应的下一级页表项
        pte = &((SV39PTE *)ppn2phyaddr(pte->ppn))[vpn[level - 1]];
    }

    // 设置最终页表项
    pte->value = 0;
    pte->v = 1;
    pte->rwx = rwx;
    pte->u = u;
    pte->g = g;
    pte->ppn = phyaddr2ppn(paddr);
}

void sv39_map_range(SV39PTE *root, void *vstart, void *pstart, size_t pages, umb_t rwx, bool u, bool g)
{
    if (sv39_alloc_page == nullptr) {
        log_error("sv39_map_range: 未设置页框分配函数");
        return;
    }

    // 逐页映射
    for (int i = 0 ; i < pages ; i ++) {
        void *vaddr = (void *)((umb_t)vstart + i * SV39_PAGE_SIZE);
        void *paddr = (void *)((umb_t)pstart + i * SV39_PAGE_SIZE);
        sv39_mapping(root, vaddr, paddr, rwx, u, g);
    }
}