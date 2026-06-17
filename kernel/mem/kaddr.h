/**
 * @file kaddr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核地址空间
 * @version alpha-1.0.0
 * @date 2026-02-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#include <sustcore/addr.h>

namespace ker_paddr {
    struct Segment {
        PhyAddr pstart, pend;
        VirAddr vstart, vend;

        Segment() = default;

        constexpr Segment(PhyAddr ps, PhyAddr pe, VirAddr vs, VirAddr ve)
            : pstart(ps), pend(pe), vstart(vs), vend(ve) {
            assert((pe - ps) == (ve - vs));
        }

        constexpr size_t size() const {
            return vend - vstart;
        }
    };
    extern Segment kernel;
    extern Segment text;
    extern Segment rodata;
    extern Segment data;
    extern Segment bss;
    extern Segment misc;

    void init();

    inline void map_seg(PageMan &man, const Segment &seg, PageMan::PageFlags flags) {
        man.map_range<true>(seg.vstart, seg.pstart, seg.size(), flags);
    }

    inline void mapping_kernel_areas(PageMan &man) {
        // TODO: 专门维持一个内核页表, 其它页表可以直接复用该内核页表,
        // 不需要二次构造
        map_seg(man, text, PageMan::page_flags(PageMan::rwx(true, false, true),
                                               false, true));
        map_seg(man, rodata,
                PageMan::page_flags(PageMan::rwx(true, false, false), false, true));
        map_seg(man, data, PageMan::page_flags(PageMan::rwx(true, true, false),
                                               false, true));
        map_seg(man, bss, PageMan::page_flags(PageMan::rwx(true, true, false),
                                              false, true));
        map_seg(man, misc, PageMan::page_flags(PageMan::rwx(true, false, false),
                                               false, true));
    }
}  // namespace ker_paddr
