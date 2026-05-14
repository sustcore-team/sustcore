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
#include <mem/kaddr.h>
#include <mem/vma.h>
#include <sus/logger.h>
#include <sus/owner.h>
#include <sus/range.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>

namespace {
    bool valid_user_area(const VirArea &varea) {
        return varea.begin <= varea.end && is_user_vaddr(varea.begin) &&
               is_user_vaddr(varea.end);
    }

    VirArea page_inner_area(const VirArea &varea) {
        VirAddr begin = varea.begin.page_align_up();
        VirAddr end   = varea.end.page_align_down();
        if (begin >= end) {
            return VirArea(begin, begin);
        }
        return VirArea(begin, end);
    }
}  // namespace

TaskMemoryManager::TaskMemoryManager(PhyAddr _pgd)
    : vma_list(), _pgd(_pgd), _pman(_pgd) {
    PageMan::make_root(_pgd);
    ker_paddr::mapping_kernel_areas(_pman);
}

TaskMemoryManager::~TaskMemoryManager() {
    auto &&list = std::move(vma_list);
    for (VMA &vma : list) {
        delete util::owner(&vma);
    }
    // TODO: 释放所有内存占用与页表
}

Result<util::nonnull<VMA *>> TaskMemoryManager::add_vma(VMA::Type type,
                                                        VMA::Growth growth,
                                                        const VirArea &varea) {
    if (!valid_user_area(varea)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    if (!varea.nullable() && locate_range(varea).has_value()) {
        unexpect_return(ErrCode::BUSY);
    }

    VMA *vma = new VMA(this, type, growth, varea);
    vma_list.push_back(*vma);
    return util::nonnull(*vma);
}

Result<util::nonnull<VMA *>> TaskMemoryManager::clone_vma(
    util::nonnull<VMA *> vma, TaskMemoryManager &dst) {
    return __check_vma(vma).and_then([&dst](VMA *vma) {
        return dst.add_vma(vma->type, vma->growth, vma->varea);
    });
}

Result<util::nonnull<VMA *>> TaskMemoryManager::locate(VirAddr vaddr) {
    for (auto &vma : vma_list) {
        if (within(vma.varea, vaddr) ||
            (vma.varea.size() == 0 && vma.varea.begin == vaddr))
        {
            return util::nonnull(vma);
        }
    }
    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
}

Result<util::nonnull<VMA *>> TaskMemoryManager::locate_range(
    const VirArea &varea) {
    for (auto &vma : vma_list) {
        if (is_intersecting(vma.varea, varea)) {
            return util::nonnull(vma);
        }
    }
    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
}

Result<void> TaskMemoryManager::remove_vma(util::nonnull<VMA *> vma) {
    return __check_vma(vma).and_then([this](VMA *vma) {
        vma_list.remove(*vma);
        delete util::owner(vma);
        void_return();
    });
}

Result<VirArea> TaskMemoryManager::grow_vma(util::nonnull<VMA *> vma,
                                            const VirArea &varea) {
    auto check_res = __check_vma(vma);
    if (!check_res.has_value()) {
        propagate_return(check_res);
    }

    VMA *target = check_res.value();
    if (!valid_user_area(varea)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    VirArea old_area = target->varea;
    if (varea == old_area) {
        return old_area;
    }

    bool grow_up   = varea.begin == old_area.begin && varea.end > old_area.end;
    bool shrink_up = varea.begin == old_area.begin && varea.end < old_area.end;
    bool grow_down = varea.end == old_area.end && varea.begin < old_area.begin;
    bool shrink_down =
        varea.end == old_area.end && varea.begin > old_area.begin;

    if (grow_up && !(target->growth & VMA::Growth::GROW_UP)) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }
    if (shrink_up && !(target->growth & VMA::Growth::SHRINK_UP)) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }
    if (grow_down && !(target->growth & VMA::Growth::GROW_DOWN)) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }
    if (shrink_down && !(target->growth & VMA::Growth::SHRINK_DOWN)) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }
    if (!grow_up && !shrink_up && !grow_down && !shrink_down) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    if (!varea.nullable()) {
        for (auto &other : vma_list) {
            if (&other != target && is_intersecting(other.varea, varea)) {
                unexpect_return(ErrCode::BUSY);
            }
        }
    }

    if (shrink_up) {
        VirArea unmap_area = page_inner_area(VirArea(varea.end, old_area.end));
        if (!unmap_area.nullable()) {
            _pman.unmap_range(unmap_area.begin, unmap_area.size());
        }
    } else if (shrink_down) {
        VirArea unmap_area =
            page_inner_area(VirArea(old_area.begin, varea.begin));
        if (!unmap_area.nullable()) {
            _pman.unmap_range(unmap_area.begin, unmap_area.size());
        }
    }

    target->varea = varea;
    _pman.flush_tlb();
    return target->varea;
}

bool TaskMemoryManager::on_np(const NoPresentEvent &e) {
    loggers::PAGING::DEBUG(
        "TM::on_np: access_address=%p, tm_pgd=%p, pman_root=%p",
        e.access_address.addr(), _pgd.addr(), _pman.get_root().addr());
    auto locate_res = locate(e.access_address);
    if (!locate_res.has_value()) {
        loggers::PAGING::ERROR(
            "TM::on_np: 地址不在任何 VMA 中: addr = %p, err=%d",
            e.access_address.addr(), locate_res.error());
        return false;
    }
    VMA *vma = locate_res.value();

    // TODO: 应在此处判断该页是否被换出
    // 如果被换出则应调用相应方法将其换入

    // 此时已经判断该页未被换出
    // 分配物理页并映射
    auto gfp_res = GFP::get_free_page(1);
    if (!gfp_res.has_value()) {
        loggers::TASK::ERROR("无法处理缺页异常: 无可用物理页");
        return false;
    }
    PhyAddr paddr = gfp_res.value();
    assert(paddr.nonnull());

    // 将虚地址向下对齐到页边界
    VirAddr aligned_vaddr = e.access_address.page_align_down();
    // 如果正在加载, 此时应当给予读写权限
    PageMan::RWX rwx =
        vma->loading ? PageMan::RWX::RW : vma->seg2rwx(vma->type);
    bool u = !vma->loading;  // 加载过程中按内核页处理，加载完成后按用户页处理

    _pman.map_page<PageMan::PageSize::_4K>(aligned_vaddr, paddr, rwx, u, false);
    _pman.flush_tlb();

    // 调试: 使用当前硬件页表根再次查询该页
    PhyAddr hw_root = PageMan::read_root();
    PageMan verify_pman(hw_root);
    auto verify_res = verify_pman.query_page(aligned_vaddr);
    if (!verify_res.has_value()) {
        loggers::PAGING::ERROR(
            "TM::on_np: 映射后在当前页表中仍查不到该页: vaddr=%p, "
            "err=%d, hw_root=%p, tm_pgd=%p",
            aligned_vaddr.addr(), verify_res.error(), hw_root.addr(),
            _pgd.addr());
    } else {
        loggers::PAGING::DEBUG(
            "TM::on_np: 页映射成功: vaddr=%p, hw_root=%p, tm_pgd=%p",
            aligned_vaddr.addr(), hw_root.addr(), _pgd.addr());
    }
    return true;
}
