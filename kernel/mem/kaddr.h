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
    extern Segment ivt;
    extern Segment rodata;
    extern Segment data;
    extern Segment bss;
    extern Segment misc;
    extern Segment kphy_space;

    void init();

    template <KernelStage Stage>
    void map_seg(_PageMan<Stage> &man, const Segment &seg, PageMan::RWX rwx,
                 bool u, bool g) {
        man.template map_range<true>(seg.vstart, seg.pstart, seg.size(), rwx, u,
                                     g);
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

    inline void sum_open() {
        csr_sstatus_t sstatus = csr_get_sstatus();
        sstatus.sum           = 1;  // 允许S-MODE访问U-MODE内存
        csr_set_sstatus(sstatus);
    }

    inline void sum_close() {
        csr_sstatus_t sstatus = csr_get_sstatus();
        sstatus.sum           = 0;  // 禁止S-MODE访问U-MODE内存
        csr_set_sstatus(sstatus);
    }

    struct SumGuard {
    private:
        bool opened = false;
    public:
        SumGuard() {
            open();
        }
        ~SumGuard() {
            close();
        }
        void open() {
            if (!opened) {
                opened = true;
                sum_open();
            }
        }
        void close() {
            if (opened) {
                opened = false;
                sum_close();
            }
        }
        // 禁止复制和移动
        SumGuard(const SumGuard &) = delete;
        SumGuard &operator=(const SumGuard &) = delete;
        SumGuard(SumGuard &&) = delete;
        SumGuard &operator=(SumGuard &&) = delete;
        // 禁止new和delete
        void *operator new(size_t) = delete;
        void operator delete(void *) = delete;
    };
}  // namespace ker_paddr