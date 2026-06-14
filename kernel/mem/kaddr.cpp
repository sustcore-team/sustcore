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

#include <env.h>
#include <sustcore/addr.h>
#include <mem/kaddr.h>
#include <symbols.h>

addr_t g_kva_offset = 0;
addr_t g_kpa_offset = 0;

namespace ker_paddr {
    Segment kernel;
    Segment text;
    Segment rodata;
    Segment data;
    Segment bss;
    Segment misc;

    Segment make_kva_seg(char *_vs, char *_ve) {
        auto vs = VirAddr(_vs);
        auto ve = VirAddr(_ve);
        auto ps = PhyAddr(_vs - KVA_OFFSET);
        auto pe = PhyAddr(_ve - KVA_OFFSET);

        return {ps, pe, vs, ve};
    }

    void init() {
        ker_paddr::kernel = make_kva_seg(&skernel, &ekernel);
        ker_paddr::text   = make_kva_seg(&s_text, &e_text);
        ker_paddr::rodata = make_kva_seg(&s_rodata, &e_rodata);
        ker_paddr::data   = make_kva_seg(&s_data, &e_data);
        ker_paddr::bss    = make_kva_seg(&s_bss, &e_bss);
        ker_paddr::misc   = make_kva_seg(&s_misc, &ekernel);
    }
}  // namespace ker_paddr