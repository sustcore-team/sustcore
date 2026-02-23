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

#include <mem/addr.h>
#include <mem/gfp.h>
#include <mem/vma.h>
#include <mem/kaddr.h>

TM::TM(void)
    : vma_list(), __pgd()
{
    ker_paddr::mapping_kernel_areas(__pgd);
}

TM::~TM()
{
    auto &&list = std::move(vma_list);
    for (VMA &vma : list) {
        delete &vma;
    }
}

void TM::add_vma(VMA::Type type, VirAddr vaddr, size_t size)
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

VMA *TM::find_vma(VirAddr vaddr)
{
    for (auto &vma : vma_list) {
        if (vaddr >= vma.vaddr && vaddr < (vma.vaddr + vma.size)) {
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

void TM::map_vma(VMA *vma, PhyAddr paddr, size_t size)
{
    assert (vma->tm == this);
    if (!VMA::mappable(vma->type)) {
        TASK::ERROR("%s类型的VMA不能被映射到页表中", to_string(vma->type));
    }
    if (size > vma->size) {
        TASK::ERROR("映射大小超过VMA的大小");
    }
    __pgd.map_range<true>(vma->vaddr, paddr, size, VMA::seg2rwx(vma->type), true, false);
}