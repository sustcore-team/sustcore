/**
 * @file pageman.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 页表管理实现
 * @version alpha-1.0.0
 * @date 2026-06-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/mem/pageman.h>
#include <arch/loongarch64/mem/paging.h>
#include <arch/loongarch64/csrnum.h>
#include <logger.h>

using namespace la64;

extern "C" void loongarch64_tlb_refill_entry(void);

namespace {
    #define _STR(x) #x
    #define STR(x) _STR(x)

    #define csr_write_imm(csr, value) \
        asm volatile("csrwr %0, " STR(csr) "" ::"r"(value))
    #define csr_read_imm(csr, value) \
        asm volatile("csrrd %0, " STR(csr) "" : "=r"(value))

    [[nodiscard]]
    bool is_table_pte(const PageMan::PTE &pte, int level) noexcept {
        return level < PageMan::level(PageMan::PageSize::_4K) - 1 &&
               PageMan::pte_exists(pte) && PageMan::pte_is_table(pte);
    }

    [[nodiscard]]
    Result<void> ensure_child_table(PageMan::PTE &dst_pte) noexcept {
        auto new_page_res = PageMan::new_page();
        propagate(new_page_res);

        PhyAddr new_pt = new_page_res.value();
        PageMan::make_root(new_pt);
        dst_pte.value = new_pt.arith() & PageMan::PAGE_ADDR_MASK;
        void_return();
    }

    [[nodiscard]]
    Result<void> merge_page_table(PageMan::PTE *dst_pt, PageMan::PTE *src_pt,
                                  int level) noexcept {
        if (dst_pt == nullptr || src_pt == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        for (size_t i = 0; i < PageMan::PTE_CNT; ++i) {
            auto &src_pte = src_pt[i];
            if (!PageMan::pte_exists(src_pte)) {
                continue;
            }

            auto &dst_pte = dst_pt[i];
            if (!PageMan::pte_exists(dst_pte)) {
                if (is_table_pte(src_pte, level)) {
                    auto child_res = ensure_child_table(dst_pte);
                    propagate(child_res);
                } else {
                    dst_pte = src_pte;
                    continue;
                }
            }

            if (!is_table_pte(src_pte, level) || !is_table_pte(dst_pte, level))
            {
                continue;
            }

            auto *dst_next = PageMan::_as<PageMan::PTE>(
                PageMan::get_physical_address(dst_pte));
            auto *src_next = PageMan::_as<PageMan::PTE>(
                PageMan::get_physical_address(src_pte));
            auto merge_res = merge_page_table(dst_next, src_next, level + 1);
            propagate(merge_res);
        }

        void_return();
    }
}  // namespace

void PageMan::init(void) {
    umb_t handler_phys = convert<PhyAddr>(
        KvaAddr(reinterpret_cast<addr_t>(&loongarch64_tlb_refill_entry)))
                            .arith() &
                        PAGE_ADDR_MASK;
    umb_t old_dmwin1;
    csr_read_imm(CSR_DMWIN1, old_dmwin1);

    csr_write_imm(CSR_DMWIN1, DMW0_CONFIG);
    asm volatile("ibar 0\n\tdbar 0" ::: "memory");

    csr_write_imm(CSR_TLBRENTRY, DMW0_BASE | handler_phys);
    csr_write_imm(CSR_PWCTL0, PWCTL0_4LEVEL);
    csr_write_imm(CSR_PWCTL1, PWCTL1_4LEVEL);
    csr_write_imm(CSR_STLBPGSIZE, STLBPGSIZE_4K);
    csr_write_imm(CSR_DMWIN0, DMW0_CONFIG);
    csr_write_imm(CSR_DMWIN1, old_dmwin1);
    flush_tlb();
    loggers::PAGING::INFO("LoongArch64页表管理器初始化完成");
}

VirAddr PageMan::_convert(PhyAddr paddr) {
    return VirAddr(DMW0_BASE | paddr.arith());
}

void PageMan::set_cow(PTE *pte, bool cow) {
    if (pte == nullptr) {
        return;
    }
    if (cow) {
        pte->value |= PAGE_COW;
    } else {
        pte->value &= ~PAGE_COW;
    }
}

void PageMan::set_paddr(PTE *pte, PhyAddr paddr) {
    if (pte == nullptr) {
        return;
    }
    pte->value = (pte->value & ~PAGE_ADDR_MASK) |
                 (paddr.arith() & PAGE_ADDR_MASK);
}

PhyAddr PageMan::read_root() {
    umb_t pgdl;
    csr_read_imm(CSR_PGDL, pgdl);
    return PhyAddr(pgdl & PAGE_ADDR_MASK);
}

void PageMan::make_root(PhyAddr root) {
    memset(_convert(root).addr(), 0, PAGESIZE);
}

void PageMan::__switch_root(PhyAddr root) {
    umb_t root_val = root.arith() & PAGE_ADDR_MASK;
    csr_write_imm(CSR_PGDL, root_val);
    csr_write_imm(CSR_PGDH, root_val);
    flush_tlb();
}

void PageMan::flush_tlb() {
    asm volatile("dbar 0\n\tinvtlb 0x0, $zero, $zero\n\tibar 0" ::: "memory");
}

Result<PageMan::QueryResult> PageMan::query_page(VirAddr vaddr) {
    umb_t vpn[level(PageSize::_4K)];
    make_vpn<PageSize::_4K>(vaddr, vpn);

    PTE *pt = root();
    for (int level_index = 0; level_index < level(PageSize::_4K); ++level_index) {
        PTE &pte = pt[vpn[level_index]];
        if (!pte_exists(pte)) {
            unexpect_return(ErrCode::PAGE_NOT_PRESENT);
        }

        if (!pte_is_table(pte)) {
            PageSize size = PageSize::_4K;
            if (level_index == 1) {
                size = PageSize::_1G;
            } else if (level_index == 2) {
                size = PageSize::_2M;
            }
            return QueryResult{
                .pte  = &pte,
                .size = size,
            };
        }

        pt = _as<PTE>(get_physical_address(pte));
    }

    unexpect_return(ErrCode::INVALID_PTE);
}

Result<void> PageMan::clone_mapping_from(PageMan &src, VirAddr vaddr) noexcept {
    auto query_res = src.query_page(vaddr);
    if (!query_res.has_value()) {
        propagate_return(query_res);
    }

    auto qres     = query_res.value();
    auto *src_pte = qres.pte;
    PhyAddr paddr = get_physical_address(*src_pte);
    PageFlags flags = page_flags(rwx(*src_pte), is_user_accessible(*src_pte),
                                 is_global(*src_pte), is_present(*src_pte));
    VirAddr mapped = vaddr.page_align_down();
    switch (qres.size) {
        case PageSize::_1G:
            map_page<PageSize::_1G>(mapped, paddr, flags);
            break;
        case PageSize::_2M:
            map_page<PageSize::_2M>(mapped, paddr, flags);
            break;
        case PageSize::_4K:
            map_page<PageSize::_4K>(mapped, paddr, flags);
            break;
        case PageSize::_NULL:
        default:
            unexpect_return(ErrCode::INVALID_PTE);
    }
    void_return();
}

Result<void> PageMan::merge_from(PageMan &src) noexcept {
    return merge_page_table(root(), src.root(), 0);
}
