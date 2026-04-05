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
#include <mem/kaddr.h>
#include <mem/vma.h>
#include <sus/logger.h>
#include <sus/owner.h>
#include <sus/range.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>

TM::TM(PhyAddr _pgd) : vma_list(), _pgd(_pgd), _pman(_pgd)
{
    PageMan::make_root(_pgd);
    ker_paddr::mapping_kernel_areas(_pman);
}

TM::~TM() {
    auto &&list = std::move(vma_list);
    for (VMA &vma : list) {
        delete util::owner(&vma);
    }
}

Result<void> TM::add_vma(VMA::Type type, VirAddr vaddr, size_t size) {
    VMA *vma = new VMA(this, type, vaddr, size);
    vma_list.push_back(*vma);
    void_return();
}

Result<void> TM::clone_vma(TM &other, VirAddr vma_addr) {
    auto locate_res = locate(vma_addr);
    if (!locate_res.has_value()) {
        unexpect_return(locate_res.error());
    }
    VMA *vma     = locate_res.value();
    VMA *new_vma = new VMA(&other, *vma);
    other.vma_list.push_back(*new_vma);
    void_return();
}

Result<util::nonnull<VMA *>> TM::locate(VirAddr vaddr) {
    using namespace util::range;
    for (auto &vma : vma_list) {
        Range<VirAddr> vma_range(vma.vaddr, vma.vaddr + vma.size);
        if (within(vma_range, vaddr)) {
            return util::nonnull_from(vma);
        }
    }
    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
}

Result<util::nonnull<VMA *>> TM::locate_range(VirAddr vaddr, size_t size) {
    using namespace util::range;
    for (auto &vma : vma_list) {
        Range<VirAddr> vma_range(vma.vaddr, vma.vaddr + vma.size);
        Range<VirAddr> query_range(vaddr, vaddr + size);
        if (is_intersecting(vma_range, query_range)) {
            return util::nonnull_from(vma);
        }
    }
    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
}

Result<void> TM::remove_vma(VirAddr vma_addr) {
    auto locate_res = locate(vma_addr);
    if (!locate_res.has_value()) {
        unexpect_return(locate_res.error());
    }
    VMA *vma = locate_res.value();
    vma_list.remove(*vma);
    delete util::owner(vma);
    void_return();
}

bool TM::on_np(const NoPresentEvent &e) {
    PAGING::DEBUG("TM::on_np: access_address=%p, tm_pgd=%p, pman_root=%p",
                  e.access_address.addr(), _pgd.addr(), _pman.get_root().addr());
    auto locate_res = locate(e.access_address);
    if (!locate_res.has_value()) {
        PAGING::ERROR("TM::on_np: 地址不在任何 VMA 中: %p, err=%d",
                      e.access_address.addr(), locate_res.error());
        return false;
    }
    VMA *vma = locate_res.value();

    // TODO: 应在此处判断该页是否被换出
    // 如果被换出则应调用相应方法将其换入

    // 此时已经判断该页未被换出
    // 分配物理页并映射
    auto gfp_res = GFP::get_free_page();
    if (!gfp_res.has_value()) {
        TASK::ERROR("无法处理缺页异常: 无可用物理页");
        return false;
    }
    PhyAddr paddr = gfp_res.value();
    assert(paddr.nonnull());

    // 将虚地址向下对齐到页边界
    VirAddr aligned_vaddr = e.access_address.page_align_down();

    _pman.map_page<PageMan::PageSize::_4K>(
        aligned_vaddr, paddr, vma->seg2rwx(vma->type), true, false);
    _pman.flush_tlb();

    // 调试: 使用当前硬件页表根再次查询该页
    PhyAddr hw_root = PageMan::read_root();
    PageMan verify_pman(hw_root);
    auto verify_res = verify_pman.query_page(aligned_vaddr);
    if (!verify_res.has_value()) {
        PAGING::ERROR(
            "TM::on_np: 映射后在当前页表中仍查不到该页: vaddr=%p, "
            "err=%d, hw_root=%p, tm_pgd=%p",
            aligned_vaddr.addr(), verify_res.error(), hw_root.addr(),
            _pgd.addr());
    } else {
        PAGING::DEBUG(
            "TM::on_np: 页映射成功: vaddr=%p, hw_root=%p, tm_pgd=%p",
            aligned_vaddr.addr(), hw_root.addr(), _pgd.addr());
    }
    return true;
}