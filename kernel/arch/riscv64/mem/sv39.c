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

#include <arch/riscv64/csr.h>
#include <arch/riscv64/mem/sv39.h>
#include <basec/logger.h>
#include <mem/kmem.h>
#include <mem/buddy.h>
#include <string.h>

void sv39_mapping_init(void) {}

SV39PT construct_sv39_mapping_root(void) {
    SV39PT root = PA2KPA((SV39PT)alloc_page());
    memset(root, 0, SV39_4K_PAGE_SIZE);
    return root;
}

SV39PT sv39_mapping_root(void) {
    csr_satp_t satp = csr_get_satp();
    if (satp.mode != SATP_MODE_SV39) {
        log_error("sv39_mapping_root: 根页表不为SV39模式!");
        return nullptr;
    }
    return PA2KPA(ppn2phyaddr(satp.ppn));
}

void sv39_maps_to(SV39PT root, void *vaddr, void *paddr, umb_t rwx, bool u,
                  bool g) {
    if (rwx == RWX_MODE_P) {
        // 不允许设置为指向下一级页表
        log_error("sv39_maps_to: 无效rwx模式RWX_MODE_P");
        return;
    }

    // 将vaddr分解为三级页表索引
    umb_t vpn[3];
    umb_t va = (umb_t)vaddr;
    vpn[0]   = (va >> 12) & 0x1FF;
    vpn[1]   = (va >> 21) & 0x1FF;
    vpn[2]   = (va >> 30) & 0x1FF;

    // 搜索第一级页表
    SV39PTE *pte = &root[vpn[2]];
    // 搜索第二级, 第三级
    for (int level = 2; level > 0; level--) {
        if (!(pte->v)) {
            SV39PT *new_table = (SV39PT *)alloc_page();
            // 初始化新的页表项
            pte->value        = 0;
            pte->v            = 1;
            pte->rwx          = RWX_MODE_P;
            pte->ppn          = phyaddr2ppn(new_table);
        } else if (pte->rwx != RWX_MODE_P) {
            log_error("sv39_maps_to: 尝试覆盖大页映射!");
            return;
        }
        // 取其中相应的下一级页表项
        // 应该使用PA2KPA进行转换
        pte = &((SV39PTE *)PA2KPA(ppn2phyaddr(pte->ppn)))[vpn[level - 1]];
    }

    // 设置最终页表项
    pte->value = 0;
    pte->v     = 1;
    pte->rwx   = rwx;
    pte->u     = u;
    pte->g     = g;
    pte->ppn   = phyaddr2ppn(paddr);

    // log_debug("sv39_maps_to: 映射4KB大页 vaddr=[%p, %p) to paddr=[%p, %p)",
    //          vaddr, (void *)((umb_t)vaddr + SV39_4K_PAGE_SIZE), paddr,
    //          (void *)((umb_t)paddr + SV39_4K_PAGE_SIZE));
}

void sv39_maps_to_2m(SV39PT root, void *vaddr, void *paddr, umb_t rwx, bool u,
                     bool g) {
    if (rwx == RWX_MODE_P) {
        // 不允许设置为指向下一级页表
        log_error("sv39_maps_to: 无效rwx模式RWX_MODE_P");
        return;
    }

    // 将vaddr分解为二级页表索引
    umb_t vpn[2];
    umb_t va = (umb_t)vaddr;
    vpn[0]   = (va >> 21) & 0x1FF;
    vpn[1]   = (va >> 30) & 0x1FF;

    // 搜索第一级页表
    SV39PTE *pte = &root[vpn[1]];
    // 搜索第二级
    if (!(pte->v)) {
        SV39PT *new_table = (SV39PT *)alloc_page();
        // 初始化新的页表项
        pte->value        = 0;
        pte->v            = 1;
        pte->rwx          = RWX_MODE_P;
        pte->ppn          = phyaddr2ppn(new_table);
    } else if (pte->rwx != RWX_MODE_P) {
        log_error("sv39_maps_to_2m: 尝试覆盖大页映射!");
        return;
    }
    // 取其中相应的下一级页表项
    pte = &((SV39PTE *)PA2KPA(ppn2phyaddr(pte->ppn)))[vpn[0]];

    // 设置最终页表项
    pte->value = 0;
    pte->v     = 1;
    pte->rwx   = rwx;
    pte->u     = u;
    pte->g     = g;
    pte->ppn   = phyaddr2ppn(paddr);

    // log_debug("sv39_maps_to_2m: 映射2MB大页 vaddr=[%p, %p) to paddr=[%p,
    // %p)",
    //          vaddr, (void *)((umb_t)vaddr + SV39_2M_PAGE_SIZE), paddr,
    //          (void *)((umb_t)paddr + SV39_2M_PAGE_SIZE));
}

void sv39_maps_to_1g(SV39PT root, void *vaddr, void *paddr, umb_t rwx, bool u,
                     bool g) {
    if (rwx == RWX_MODE_P) {
        // 不允许设置为指向下一级页表
        log_error("sv39_maps_to: 无效rwx模式RWX_MODE_P");
        return;
    }

    // 将vaddr分解为二级页表索引
    umb_t va  = (umb_t)vaddr;
    umb_t vpn = (va >> 30) & 0x1FF;

    // 搜索第一级页表
    SV39PTE *pte = &root[vpn];

    // 设置最终页表项
    pte->value = 0;
    pte->v     = 1;
    pte->rwx   = rwx;
    pte->u     = u;
    pte->g     = g;
    pte->ppn   = phyaddr2ppn(paddr);

    // log_debug("sv39_maps_to_1g: 映射1GB大页 vaddr=[%p, %p) to paddr=[%p,
    // %p)",
    //          vaddr, (void *)((umb_t)vaddr + SV39_1G_PAGE_SIZE), paddr,
    //          (void *)((umb_t)paddr + SV39_1G_PAGE_SIZE));
}

void sv39_maps_range_to(SV39PT root, void *vstart, void *pstart, size_t pages,
                        umb_t rwx, bool u, bool g, bool pagewise) {
    if (pagewise) {
        // 逐页映射
        for (int i = 0; i < pages; i++) {
            void *vaddr = (void *)((umb_t)vstart + i * SV39_PAGE_SIZE);
            void *paddr = (void *)((umb_t)pstart + i * SV39_PAGE_SIZE);
            sv39_maps_to(root, vaddr, paddr, rwx, u, g);
        }
        return;
    }
    // 尝试使用大页映射
    const size_t pages_per_1g = SV39_1G_PAGE_SIZE / SV39_PAGE_SIZE;
    const size_t pages_per_2m = SV39_2M_PAGE_SIZE / SV39_PAGE_SIZE;

    // 记录当前进度
    size_t pages_remaining = pages;
    void *vaddr            = vstart;
    void *paddr            = pstart;

    // 寻找目前能够映射的最大页面
    // 注意地址对齐要求
    while (pages_remaining > 0) {
        // 如果均是1GB对齐且剩余页数足够映射1GB
        if (((umb_t)vaddr % SV39_1G_PAGE_SIZE == 0) &&
            ((umb_t)paddr % SV39_1G_PAGE_SIZE == 0) &&
            (pages_remaining >= pages_per_1g))
        {
            // 映射1GB大页
            sv39_maps_to_1g(root, vaddr, paddr, rwx, u, g);
            vaddr            = (void *)((umb_t)vaddr + SV39_1G_PAGE_SIZE);
            paddr            = (void *)((umb_t)paddr + SV39_1G_PAGE_SIZE);
            pages_remaining -= pages_per_1g;
            // 如果均是2MB对齐且剩余页数足够映射2MB
        } else if (((umb_t)vaddr % SV39_2M_PAGE_SIZE == 0) &&
                   ((umb_t)paddr % SV39_2M_PAGE_SIZE == 0) &&
                   (pages_remaining >= pages_per_2m))
        {
            // 映射2MB大页
            sv39_maps_to_2m(root, vaddr, paddr, rwx, u, g);
            vaddr            = (void *)((umb_t)vaddr + SV39_2M_PAGE_SIZE);
            paddr            = (void *)((umb_t)paddr + SV39_2M_PAGE_SIZE);
            pages_remaining -= pages_per_2m;
        } else {
            // 映射4KB小页
            sv39_maps_to(root, vaddr, paddr, rwx, u, g);
            vaddr            = (void *)((umb_t)vaddr + SV39_PAGE_SIZE);
            paddr            = (void *)((umb_t)paddr + SV39_PAGE_SIZE);
            pages_remaining -= 1;
        }
    }
}

int sv39_modify_page_flags(SV39PT root, void *vaddr, int mask, umb_t rwx,
                           bool u, bool g) {
    // 将vaddr分解为三级页表索引
    umb_t vpn[3];
    umb_t va = (umb_t)vaddr;
    vpn[0]   = (va >> 12) & 0x1FF;
    vpn[1]   = (va >> 21) & 0x1FF;
    vpn[2]   = (va >> 30) & 0x1FF;

    int lvl = -1;

    // 搜索第一级页表
    SV39PTE *pte = &root[vpn[2]];
    // 搜索第二级, 第三级
    for (int level = 2; level > 0; level--) {
        // 如果是无效页表项
        if (!(pte->v)) {
            log_error("sv39_modify_page_flags: 页表项无效!");
            return -1;
        }
        if (pte->rwx != RWX_MODE_P) {
            // 这是一个大页
            lvl = level;
            break;
        }
        // 取其中相应的下一级页表项
        pte = &((SV39PTE *)ppn2phyaddr(pte->ppn))[vpn[level - 1]];
    }

    if (pte == nullptr || !(pte->v) || (pte->rwx == RWX_MODE_P)) {
        log_error("sv39_modify_page_flags: 页表项无效!");
        return -1;
    }

    lvl = (lvl == -1) ? 0 : lvl;

    // 修改页表项标志
    if (mask & 1) {
        pte->rwx = rwx;
    }
    if (mask & 2) {
        pte->u = u;
    }
    if (mask & 4) {
        pte->g = g;
    }

    return lvl;
}

void sv39_modify_page_range_flags(SV39PT root, void *vstart, void *vend,
                                  int mask, umb_t rwx, bool u, bool g) {
    while ((umb_t)vstart < (umb_t)vend) {
        int lvl = sv39_modify_page_flags(root, vstart, mask, rwx, u, g);
        switch (lvl) {
            case -1:
                log_error("sv39_modify_page_range_flags: 修改页面标志失败!");
                // 修改失败, 直接返回
                return;
            case 0:
                // 4KB页面
                vstart = (void *)((umb_t)vstart + SV39_4K_PAGE_SIZE);
                break;
            case 1:
                // 2MB页面
                vstart = (void *)((umb_t)vstart + SV39_2M_PAGE_SIZE);
                break;
            case 2:
                // 1GB页面
                vstart = (void *)((umb_t)vstart + SV39_1G_PAGE_SIZE);
                break;
            default:
                // 不可能发生
                log_error("sv39_modify_page_range_flags: 未知页面级别!");
                return;
        }
    }
}

SV39LargablePTE sv39_get_pte(SV39PT root, void *vaddr) {
    // 将vaddr分解为三级页表索引
    umb_t vpn[3];
    umb_t va = (umb_t)vaddr;
    vpn[0]   = (va >> 12) & 0x1FF;
    vpn[1]   = (va >> 21) & 0x1FF;
    vpn[2]   = (va >> 30) & 0x1FF;

    // 搜索第一级页表
    SV39PTE *pte = &root[vpn[2]];
    // 搜索第二级, 第三级
    for (int level = 2; level > 0; level--) {
        // 如果是无效页表项
        if (!(pte->v)) {
            return (SV39LargablePTE){.entry = nullptr, .level = -1};
        }
        if (pte->rwx != RWX_MODE_P) {
            // 这是一个大页
            return (SV39LargablePTE){.entry = pte, .level = level};
        }
        // 取其中相应的下一级页表项
        pte = &((SV39PTE *)PA2KPA(ppn2phyaddr(pte->ppn)))[vpn[level - 1]];
    }

    return (SV39LargablePTE){.entry = pte, .level = 0};
}

static void print_mapping(void *vaddr, void *paddr, size_t size) {
    // 注意, 高位为1时要作sign-extend处理
    if (((umb_t)vaddr >> 38) & 1) {
        vaddr = (void *)((0xFFFF'FFC0'0000'000ul) | (umb_t)vaddr);
    }
    if (((umb_t)paddr >> 38) & 1) {
        paddr = (void *)((0xFFFF'FFC0'0000'000ul) | (umb_t)paddr);
    }
    log_info("[%p, %p) => [%p, %p)", vaddr, (void *)((umb_t)vaddr + size),
             paddr, (void *)((umb_t)paddr + size));
}

static bool continue_segment(void *vaddr1, void *paddr1, void *vaddr2,
                             void *paddr2, size_t size) {
    return (((umb_t)vaddr1 + size) == (umb_t)vaddr2) &&
           (((umb_t)paddr1 + size) == (umb_t)paddr2);
}

// 打印SV39页表映射布局
void sv39_print_mapping_layout(SV39PT root) {
    // 目前遇到的最长连续映射段
    void *seg_vstart = nullptr;
    void *seg_pstart = nullptr;
    size_t seg_size  = 0;

    if (root == nullptr) {
        log_info("sv39_print_mapping_layout: 页表根为空!");
        return;
    }

    // 遍历三级页表
    for (umb_t vpn2 = 0; vpn2 < SV39_PTE_COUNT; vpn2++) {
        SV39PTE *pte2 = &root[vpn2];
        // 无效页表项
        if (!(pte2->v)) {
            // 如果有未打印的段, 则打印
            if (seg_size > 0) {
                print_mapping(seg_vstart, seg_pstart, seg_size);
                seg_size   = 0;
                seg_vstart = seg_pstart = nullptr;
            }
            continue;
        }
        // 1GB大页
        if (pte2->rwx != RWX_MODE_P) {
            void *vaddr = (void *)((vpn2 << 30));
            void *paddr = ppn2phyaddr(pte2->ppn);
            if (seg_vstart == nullptr) {
                // 开始新的段
                seg_vstart = vaddr;
                seg_pstart = paddr;
                seg_size   = SV39_1G_PAGE_SIZE;
                continue;
            }

            // 检查是否可以合并到当前段
            if (continue_segment(seg_vstart, seg_pstart, vaddr, paddr,
                                 seg_size))
            {
                seg_size += SV39_1G_PAGE_SIZE;
            } else {
                // 打印当前段
                if (seg_size > 0) {
                    print_mapping(seg_vstart, seg_pstart, seg_size);
                    seg_size   = 0;
                    seg_vstart = seg_pstart = nullptr;
                }
                // 开始新的段
                seg_vstart = vaddr;
                seg_pstart = paddr;
                seg_size   = SV39_1G_PAGE_SIZE;
            }
            continue;
        }
        // 二级页表
        SV39PT pgt1 = (SV39PT)PA2KPA(ppn2phyaddr(pte2->ppn));
        for (umb_t vpn1 = 0; vpn1 < SV39_PTE_COUNT; vpn1++) {
            SV39PTE *pte1 = &pgt1[vpn1];
            // 无效页表项
            if (!(pte1->v)) {
                // 如果有未打印的段, 则打印
                if (seg_size > 0) {
                    print_mapping(seg_vstart, seg_pstart, seg_size);
                    seg_size   = 0;
                    seg_vstart = seg_pstart = nullptr;
                }
                continue;
            }
            // 2MB大页
            if (pte1->rwx != RWX_MODE_P) {
                void *vaddr = (void *)((vpn2 << 30) | (vpn1 << 21));
                void *paddr = ppn2phyaddr(pte1->ppn);
                if (seg_vstart == nullptr) {
                    // 开始新的段
                    seg_vstart = vaddr;
                    seg_pstart = paddr;
                    seg_size   = SV39_2M_PAGE_SIZE;
                    continue;
                }
                // 检查是否可以合并到当前段
                if (continue_segment(seg_vstart, seg_pstart, vaddr, paddr,
                                     seg_size))
                {
                    seg_size += SV39_2M_PAGE_SIZE;
                } else {
                    // 打印当前段
                    if (seg_size > 0) {
                        print_mapping(seg_vstart, seg_pstart, seg_size);
                        seg_size   = 0;
                        seg_vstart = seg_pstart = nullptr;
                    }
                    // 开始新的段
                    seg_vstart = vaddr;
                    seg_pstart = paddr;
                    seg_size   = SV39_2M_PAGE_SIZE;
                }
                continue;
            }
            // 三级页表
            SV39PT pgt0 = (SV39PT)PA2KPA(ppn2phyaddr(pte1->ppn));
            for (umb_t vpn0 = 0; vpn0 < SV39_PTE_COUNT; vpn0++) {
                SV39PTE *pte0 = &pgt0[vpn0];
                // 无效页表项
                if (!(pte0->v)) {
                    // 如果有未打印的段, 则打印
                    if (seg_size > 0) {
                        print_mapping(seg_vstart, seg_pstart, seg_size);
                        seg_size   = 0;
                        seg_vstart = seg_pstart = nullptr;
                    }
                    continue;
                }
                // 4KB小页
                void *vaddr =
                    (void *)((vpn2 << 30) | (vpn1 << 21) | (vpn0 << 12));
                void *paddr = ppn2phyaddr(pte0->ppn);
                if (seg_vstart == nullptr) {
                    // 开始新的段
                    seg_vstart = vaddr;
                    seg_pstart = paddr;
                    seg_size   = SV39_4K_PAGE_SIZE;
                    continue;
                }
                // 检查是否可以合并到当前段
                if (continue_segment(seg_vstart, seg_pstart, vaddr, paddr,
                                     seg_size))
                {
                    seg_size += SV39_4K_PAGE_SIZE;
                } else {
                    // 打印当前段
                    if (seg_size > 0) {
                        print_mapping(seg_vstart, seg_pstart, seg_size);
                        seg_size   = 0;
                        seg_vstart = seg_pstart = nullptr;
                    }
                    // 开始新的段
                    seg_vstart = vaddr;
                    seg_pstart = paddr;
                    seg_size   = SV39_4K_PAGE_SIZE;
                }
            }
        }
    }
}