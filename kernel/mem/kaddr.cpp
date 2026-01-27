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

#include <configuration.h>
#include <mem/kaddr.h>
#include <symbols.h>

namespace ker_paddr {
    struct Segment {
        void *start, *end;
        void *vstart, *vend;

        Segment() = default;

        Segment(void *s, void *e)
            : start(s), end(e), vstart(PA2KA(s)), vend(PA2KA(e)) {}

        Segment(void *s, void *e, void *vs, void *ve)
            : start(s), end(e), vstart(vs), vend(ve) {}

        size_t size() const {
            return (char *)end - (char *)start;
        }
    };
    Segment kernel;
    Segment text;
    Segment ivt;
    Segment rodata;
    Segment data;
    Segment bss;
    Segment misc;

    Segment kphy_space;

    void init_ker_paddr(void *upper_bound) {
        ker_paddr::kernel = Segment(&skernel, &ekernel);
        ker_paddr::text   = Segment(&s_text, &e_text);
        ker_paddr::ivt    = Segment(&s_ivt, &e_ivt);
        ker_paddr::rodata = Segment(&s_rodata, &e_rodata);
        ker_paddr::data   = Segment(&s_data, &e_data);
        ker_paddr::bss    = Segment(&s_bss, &e_bss);
        ker_paddr::misc   = Segment(&s_misc, &ekernel);

        ker_paddr::kphy_space =
            Segment((void *)0, upper_bound, PA2KPA(0), PA2KPA(upper_bound));
    }
}  // namespace ker_paddr

void init_ker_paddr(void *phymem_upper_bound) {
    ker_paddr::init_ker_paddr(phymem_upper_bound);
}

void map_seg(PageMan &man, const ker_paddr::Segment &seg, PageMan::RWX rwx,
             bool u, bool g) {
    man.map_range<true>(seg.vstart, seg.start, seg.size(), rwx, u, g);
}

void mapping_kernel_areas(PageMan &man) {
    using namespace ker_paddr;

    map_seg(man, ker_paddr::text, PageMan::rwx(true, false, true), false, true);
    map_seg(man, ker_paddr::ivt, PageMan::rwx(true, true, true), false,
            true);
    map_seg(man, ker_paddr::rodata, PageMan::rwx(true, false, false),
            false, true);
    map_seg(man, ker_paddr::data, PageMan::rwx(true, true, false), false,
            true);
    map_seg(man, ker_paddr::bss, PageMan::rwx(true, true, false), false,
            true);
    map_seg(man, ker_paddr::misc, PageMan::rwx(true, false, false), false,
            true);
    map_seg(man, ker_paddr::kphy_space, PageMan::rwx(true, true, false),
            false, true);
}