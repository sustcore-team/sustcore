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
#include <logger.h>
#include <sustcore/addr.h>

using namespace rv64;

namespace {
    [[nodiscard]]
    bool is_table_pte(const SV39PageMan::PTE &pte, int level) noexcept {
        return level < SV39PageMan::level(SV39PageMan::PageSize::_4K) - 1 &&
               pte.v && !pte.np && pte.rwx == SV39PageMan::RWX::P;
    }

    [[nodiscard]]
    Result<void> ensure_child_table(SV39PageMan::PTE &dst_pte) noexcept {
        auto new_page_res = SV39PageMan::new_page();
        propagate(new_page_res);

        PhyAddr new_pt = new_page_res.value();
        SV39PageMan::make_root(new_pt);
        dst_pte.value = 0;
        dst_pte.ppn   = SV39PageMan::to_ppn(new_pt);
        dst_pte.v     = true;
        dst_pte.rwx   = rwx_cast(SV39PageMan::RWX::P);
        void_return();
    }

    [[nodiscard]]
    Result<void> merge_page_table(SV39PageMan::PTE *dst_pt,
                                  SV39PageMan::PTE *src_pt,
                                  int level) noexcept {
        if (dst_pt == nullptr || src_pt == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        for (size_t i = 0; i < SV39PageMan::PTE_CNT; ++i) {
            auto &src_pte = src_pt[i];
            if (!SV39PageMan::is_valid(src_pte)) {
                continue;
            }

            auto &dst_pte = dst_pt[i];
            if (!SV39PageMan::is_valid(dst_pte)) {
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

            auto *dst_next = SV39PageMan::_as<SV39PageMan::PTE>(
                SV39PageMan::from_ppn(dst_pte.ppn));
            auto *src_next = SV39PageMan::_as<SV39PageMan::PTE>(
                SV39PageMan::from_ppn(src_pte.ppn));
            auto merge_res = merge_page_table(dst_next, src_next, level + 1);
            propagate(merge_res);
        }

        void_return();
    }
}  // namespace

void SV39PageMan::init(void) {
    loggers::PAGING::INFO("SV39页表管理器初始化完成");
}

KpaAddr SV39PageMan::_convert(PhyAddr paddr) {
    return convert<KpaAddr>(paddr);
}

void SV39PageMan::make_root(PhyAddr root) {
    memset(_convert(root).addr(), 0, PAGESIZE);
}

Result<void> SV39PageMan::merge_from(SV39PageMan &src) noexcept {
    return merge_page_table(root(), src.root(), 0);
}

void SV39PageMan::__switch_root(PhyAddr __root) {
    csr_satp_t new_satp;
    new_satp.mode = SATPMode::SV39;
    new_satp.asid = 0;  // TODO: ASID支持
    new_satp.ppn  = SV39PageMan::to_ppn(__root);
    csr_set_satp(new_satp);
}

void SV39PageMan::flush_tlb() {
    asm volatile("sfence.vma");
}
