/**
 * @file pmm.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 物理内存管理器
 * @version alpha-1.0.0
 * @date 2026-01-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/pmm.h>
#include <mem/addr.h>
#include <cassert>

PMM::page *PMM::__base_address;
size_t PMM::__arraysz;
umb_t PMM::__lower_ppn, PMM::__upper_ppn;

void PMM::init(void *lowerbound, void *upperbound) {
    __lower_ppn    = phys2ppn((umb_t)lowerbound);
    __upper_ppn    = phys2ppn((umb_t)upperbound);
    // pages
    size_t pagecnt = (__upper_ppn - __lower_ppn) / PAGESIZE;
    // 物理页信息队列大小
    __arraysz      = pagecnt * sizeof(page);
    // 转化为页数并申请内存
    __base_address =
        (page *)GFP::alloc_frame(page_align_up(__arraysz) / PAGESIZE);
    // 初始化
    memset(__base_address, 0, __arraysz);
    for (int i = 0; i < pagecnt; i++) {
        __base_address[i].ppn = i + __lower_ppn;
    }
}

PMM::page *PMM::__get_page(umb_t ppn) {
    assert(ppn >= __lower_ppn && ppn < __upper_ppn);
    return &__base_address[ppn - __lower_ppn];
}

void PMM::__ref_page(page *pg) {
    pg->refcnt++;
}

bool PMM::__unref_page(page *pg) {
    assert(pg->refcnt > 0);
    pg->refcnt--;
    if (pg->refcnt == 0) {
        return true;
    }
    return false;
}

void PMM::reset_page(page *page) {
    page->mapcnt = page->refcnt = 0;
}