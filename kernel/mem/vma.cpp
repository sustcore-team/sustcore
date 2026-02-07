/**
 * @file vma.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 虚拟内存区域
 * @version alpha-1.0.0
 * @date 2026-02-01
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <mem/gfp.h>
#include <mem/vma.h>
#include <mem/kaddr.h>

TM::TM(void)
    : vma_list(), __pgd()
{
    ker_paddr::mapping_kernel_areas(__pgd);
}

void TM::add_vma(VMA::Type type, void *vaddr, size_t size)
{
    VMA *vma = new VMA(this, type, vaddr, size);
    vma_list.push_back(*vma);
}

void TM::clone_vma(TM &other, VMA *vma)
{
    assert (vma->tm == this);
    VMA *new_vma = new VMA(&other, *vma);
    other.vma_list.push_back(*new_vma);
}

VMA *TM::find_vma(void *vaddr)
{
    for (auto &vma : vma_list) {
        if (vaddr >= vma.vaddr && vaddr < (void *)((umb_t)vma.vaddr + vma.size)) {
            return &vma;
        }
    }
    return nullptr;
}

void TM::remove_vma(VMA *vma)
{
    assert (vma->tm == this);
    vma_list.remove(*vma);
    delete vma;
}