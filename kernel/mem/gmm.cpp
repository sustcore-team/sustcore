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
#include <mem/addr.h>
#include <cassert>
#include <cstring>

void GMM::init() {
}

PhyAddr GMM::get_page(int cnt) {
    PhyAddr paddr = GFP::get_free_page(cnt);
    for (size_t i = 0 ; i < cnt ; i ++) {
        PhyAddr _paddr = paddr + i * PAGESIZE;
        PMM::page *page = PMM::get_page(_paddr);
        assert(! PMM::__refering(page));
        PMM::reset_page(page);
        PMM::__ref_page(page);
    }
    return paddr;
}

void GMM::put_page(PhyAddr paddr, int cnt) {
    for (size_t i = 0 ; i < cnt ; i ++) {
        PhyAddr _paddr = paddr + i * PAGESIZE;
        PMM::page *page = PMM::get_page(_paddr);
        assert(PMM::__refering(page));
        bool flag = PMM::__unref_page(page);
        // 引用计数 <= 0, 释放页
        // NOTE: 也许把连续的部分一次性释放更有利于性能?
        if (flag) {
            GFP::put_page(_paddr, 1);
        }
    }
}

PhyAddr GMM::clone_page(PhyAddr paddr, int cnt) {
    // clone_page() 的一个简单实现
    // 获取这些页并将原内容复制进去
    PhyAddr new_addr = get_page(cnt);
    KpaAddr vaddr = convert<KpaAddr>(paddr);
    KpaAddr new_vaddr = convert<KpaAddr>(new_addr);

    memcpy(new_vaddr.addr(), vaddr.addr(), cnt * PAGESIZE);
    return new_addr;
}