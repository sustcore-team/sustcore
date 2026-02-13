/**
 * @file sv39.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief sv39页表管理实现
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/mem/sv39.h>
#include <kio.h>

void Riscv64SV39PageMan::pre_init(void) {
    // SV39页表管理器不需要特殊的前初始化步骤
    // 但我们可以在这里设置一些全局状态或日志
    PAGING::INFO("SV39页表管理器前初始化完成");
}

void Riscv64SV39PageMan::post_init(void) {
    // SV39页表管理器不需要特殊的后初始化步骤
    // 但我们可以在这里设置一些全局状态或日志
    PAGING::INFO("SV39页表管理器后初始化完成");
}

auto Riscv64SV39PageMan::read_root(void) -> PTE * {
    csr_satp_t satp = csr_get_satp();
    if (satp.mode != SATPMode::SV39) {
        return nullptr;
    }
    umb_t root_ppn = satp.ppn;
    return (PTE *)PA2KPA(from_ppn(root_ppn));
}

auto Riscv64SV39PageMan::make_root(void) -> PTE * {
    PTE *root = (PTE *)PA2KPA(GFP::alloc_frame());
    memset(root, 0, PAGESIZE);
    return root;
}

Riscv64SV39PageMan::Riscv64SV39PageMan() : _root(make_root()) {}

auto Riscv64SV39PageMan::query_page(void *vaddr) -> ExtendedPTE {
    // 将vaddr拆分为三级索引
    umb_t vpn[3];
    umb_t va = (umb_t)vaddr;
    vpn[0]   = (va >> 12) & 0x1FF;  // VPN[0]: bits 12-20
    vpn[1]   = (va >> 21) & 0x1FF;  // VPN[1]: bits 21-29
    vpn[2]   = (va >> 30) & 0x1FF;  // VPN[2]: bits 30-38

    // TODO: check _root not null
    PTE *pte = &(root()[vpn[2]]);
    for (int level = 2; level > 0; level--) {
        if (!pte->v) {
            return {nullptr, PageSize::SIZE_NULL};
        }
        if (pte->rwx != RWX::P) {
            // 大页映射
            PageSize size =
                (level == 2) ? PageSize::SIZE_1G : PageSize::SIZE_2M;
            return {pte, size};
        }

        // 否则, 取下一级页表
        PTE *pt = (PTE *)PA2KPA(from_ppn(pte->ppn));
        // TODO: check pt not null
        pte     = &pt[vpn[level - 1]];
        assert(level - 1 >= 0);
    }
    return {pte, PageSize::SIZE_4K};
}

void Riscv64SV39PageMan::unmap_page(void *vaddr) {
    // TODO: implement unmap_page
    PAGING::ERROR("unmap_page尚未实现");
}

void Riscv64SV39PageMan::unmap_range(void *vstart, size_t size) {
    // TODO: implement unmap_range
    PAGING::ERROR("unmap_range尚未实现");
}