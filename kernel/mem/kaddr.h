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
#include <mem/addr.h>

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
    extern Segment ivt;
    extern Segment rodata;
    extern Segment data;
    extern Segment bss;
    extern Segment misc;
    extern Segment kphy_space;

    void init(PhyAddr lower_bound, PhyAddr upper_bound);

    template <KernelStage Stage>
    void map_seg(_PageMan<Stage> &man, const Segment &seg, PageMan::RWX rwx,
                 bool u, bool g) {
        man.template map_range<true>(seg.vstart, seg.pstart, seg.size(), rwx, u, g);
    }

    template <KernelStage Stage>
    void mapping_kernel_areas(_PageMan<Stage> &man) {
        // TODO: 专门维持一个内核页表, 其它页表可以直接复用该内核页表,
        // 不需要二次构造
        map_seg(man, text, PageMan::rwx(true, false, true), false, true);
        map_seg(man, ivt, PageMan::rwx(true, false, true), false, true);
        map_seg(man, rodata, PageMan::rwx(true, false, false), false, true);
        map_seg(man, data, PageMan::rwx(true, true, false), false, true);
        map_seg(man, bss, PageMan::rwx(true, true, false), false, true);
        map_seg(man, misc, PageMan::rwx(true, false, false), false, true);
        map_seg(man, kphy_space, PageMan::rwx(true, true, false), false, true);
    }
}  // namespace ker_paddr