/**
 * @file kaddr.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核地址空间
 * @version alpha-1.0.0
 * @date 2026-01-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/addr.h>
#include <mem/kaddr.h>
#include <symbols.h>

addr_t g_kva_offset = 0;
addr_t g_kpa_offset = 0;

namespace ker_paddr {
    Segment kernel;
    Segment text;
    Segment ivt;
    Segment rodata;
    Segment data;
    Segment bss;
    Segment misc;
    Segment kphy_space;

    Segment make_kva_seg(void *_ps, void *_pe) {
        PhyAddr ps = (PhyAddr)_ps;
        PhyAddr pe = (PhyAddr)_pe;
        VirAddr vs = (VirAddr)(ps.arith() + KVA_OFFSET);
        VirAddr ve = (VirAddr)(pe.arith() + KVA_OFFSET);
        return Segment(ps, pe, vs, ve);
    }

    Segment make_kpa_seg(PhyAddr ps, PhyAddr pe) {
        VirAddr vs = (VirAddr)(ps.arith() + KPA_OFFSET);
        VirAddr ve = (VirAddr)(pe.arith() + KPA_OFFSET);
        return Segment(ps, pe, vs, ve);
    }

    void init(PhyAddr lower_bound, PhyAddr upper_bound) {
        ker_paddr::kernel = make_kva_seg(&skernel, &ekernel);
        ker_paddr::text   = make_kva_seg(&s_text, &e_text);
        ker_paddr::ivt    = make_kva_seg(&s_ivt, &e_ivt);
        ker_paddr::rodata = make_kva_seg(&s_rodata, &e_rodata);
        ker_paddr::data   = make_kva_seg(&s_data, &e_data);
        ker_paddr::bss    = make_kva_seg(&s_bss, &e_bss);
        ker_paddr::misc   = make_kva_seg(&s_misc, &ekernel);
        ker_paddr::kphy_space = make_kpa_seg(lower_bound, upper_bound);
    }
}  // namespace ker_paddr