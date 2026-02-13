/**
 * @file gmm.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 通用内存管理器
 * @version alpha-1.0.0
 * @date 2026-01-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <mem/gmm.h>
#include <mem/pmm.h>
#include <mem/kaddr_defs.h>
#include <cassert>

void GMM::init() {
}

void *GMM::get_page(int cnt) {
    void *paddr = GFP::alloc_frame(cnt);
    for (int i = 0 ; i < cnt ; i ++) {
        umb_t _paddr = (umb_t) paddr + i * PAGESIZE;
        PMM::page *page = PMM::get_page((void *)_paddr);
        assert(! PMM::refering(page));
        PMM::reset_page(page);
        PMM::ref_page(page);
    }
    return paddr;
}

void GMM::put_page(void *paddr, int cnt) {
    for (int i = 0 ; i < cnt ; i ++) {
        umb_t _paddr = (umb_t) paddr + i * PAGESIZE;
        PMM::page *page = PMM::get_page((void *)_paddr);
        assert(PMM::refering(page));
        bool flag = PMM::unref_page(page);
        // 引用计数 <= 0, 释放页
        // NOTE: 也许把连续的部分一次性释放更有利于性能?
        if (flag) {
            GFP::free_frame((void *)_paddr, 1);
        }
    }
}

void *GMM::clone_page(void *paddr, int cnt) {
    // clone_page() 的一个简单实现
    // 获取这些页并将原内容复制进去
    void *new_addr = get_page(cnt);
    void *vaddr = PA2KPA(paddr);
    void *new_vaddr = PA2KPA(new_addr);

    memcpy(new_vaddr, vaddr, cnt * PAGESIZE);
    return new_addr;
}