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

#include <cstring>

namespace {
    bool valid_user_area(const VirArea &varea) {
        return varea.begin <= varea.end && is_user_vaddr(varea.begin) &&
               is_user_vaddr(varea.end);
    }

    // 寻找VArea覆盖的最大Page对齐的区域
    VirArea page_inner_area(const VirArea &varea) {
        VirAddr begin = varea.begin.page_align_up();
        VirAddr end   = varea.end.page_align_down();
        if (begin >= end) {
            return {begin, begin};
        }
        return {begin, end};
    }

    // 寻找覆盖VArea的最小Page对齐区域
    VirArea page_outer_area(const VirArea &varea) {
        if (varea.nullable()) {
            return {varea.begin, varea.begin};
        }
        return {varea.begin.page_align_down(), varea.end.page_align_up()};
    }

    size_t memory_offset_for_page(const VMA &vma, VirAddr aligned_vaddr) {
        if (aligned_vaddr <= vma.varea.begin) {
            return vma.mem_offset;
        }
        return vma.mem_offset + (aligned_vaddr - vma.varea.begin);
    }
}  // namespace

TaskMemoryManager::TaskMemoryManager(PhyAddr _pgd)
    : vma_list(), _pgd(_pgd), _pman(_pgd) {
    PageMan::make_root(_pgd);
    ker_paddr::mapping_kernel_areas(_pman);
}

TaskMemoryManager::TaskMemoryManager(ExistingPgdTag, PhyAddr _pgd)
    : vma_list(), _pgd(_pgd), _pman(_pgd) {}

Result<util::owner<TaskMemoryManager *>> TaskMemoryManager::from_existing_pgd(
    PhyAddr pgd) noexcept {
    if (!pgd.nonnull()) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto *tmm = new TaskMemoryManager(ExistingPgdTag{}, pgd);
    if (tmm == nullptr) {
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }
    return util::owner<TaskMemoryManager *>(tmm);
}

TaskMemoryManager::~TaskMemoryManager() {
    while (!vma_list.empty()) {
        VMA &vma = vma_list.front();
        vma_list.pop_front();
        unmap_pages(vma.varea);
        delete util::owner(&vma);
    }
    _pman.flush_tlb();
    // TODO: 释放页表
}

Result<util::nonnull<VMA *>> TaskMemoryManager::add_vma(
    VMA::Type type, VMA::Growth growth, const VirArea &varea,
    cap::MemoryPayload *memory, PageMan::RWX rwx, size_t mem_offset) {
    if (memory == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }
    if (!valid_user_area(varea)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    if (!varea.nullable() && locate_range(varea).has_value()) {
        unexpect_return(ErrCode::BUSY);
    }

    VMA *vma = new VMA(this, type, growth, varea, memory, rwx, mem_offset);
    vma_list.push_back(*vma);
    return util::nonnull(*vma);
}

Result<util::nonnull<VMA *>> TaskMemoryManager::clone_vma(
    util::nonnull<VMA *> vma, TaskMemoryManager &dst) {
    return __check_vma(vma).and_then([&dst](VMA *vma) {
        return dst.add_vma(vma->type, vma->growth, vma->varea, vma->memory,
                           vma->rwx, vma->mem_offset);
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

Result<util::nonnull<VMA *>> TaskMemoryManager::locate_memory(
    cap::MemoryPayload *memory, VirAddr vaddr) {
    for (auto &vma : vma_list) {
        if (vma.memory == memory &&
            (within(vma.varea, vaddr) ||
             (vma.varea.size() == 0 && vma.varea.begin == vaddr)))
        {
            return util::nonnull(vma);
        }
    }
    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
}

Result<void> TaskMemoryManager::remove_vma(util::nonnull<VMA *> vma) {
    return __check_vma(vma).and_then([this](VMA *vma) {
        unmap_pages(vma->varea);
        vma_list.remove(*vma);
        delete util::owner(vma);
        _pman.flush_tlb();
        void_return();
    });
}

void TaskMemoryManager::unmap_pages(const VirArea &varea) {
    VirArea map_area = page_outer_area(varea);
    if (map_area.nullable()) {
        return;
    }

    size_t page_count = map_area.size() / PAGESIZE;
    for (size_t i = 0; i < page_count; ++i) {
        VirAddr vaddr  = map_area.begin + i * PAGESIZE;
        auto query_res = _pman.query_page(vaddr);
        if (!query_res.has_value()) {
            continue;
        }

        _pman.unmap_page(vaddr);
    }
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
            unmap_pages(unmap_area);
        }
    } else if (shrink_down) {
        VirArea unmap_area =
            page_inner_area(VirArea(old_area.begin, varea.begin));
        if (!unmap_area.nullable()) {
            unmap_pages(unmap_area);
        }
    }

    target->varea = varea;
    _pman.flush_tlb();
    return target->varea;
}

bool TaskMemoryManager::has_memory_mapping(cap::MemoryPayload *memory) const {
    for (auto &vma : vma_list) {
        if (vma.memory == memory) {
            return true;
        }
    }
    return false;
}

Result<void> TaskMemoryManager::protect_memory_cow(cap::MemoryPayload *memory) {
    if (memory == nullptr || memory->shared) {
        void_return();
    }
    for (auto &vma : vma_list) {
        if (vma.memory != memory) {
            continue;
        }
        VirArea map_area  = page_outer_area(vma.varea);
        size_t page_count = map_area.size() / PAGESIZE;
        for (size_t i = 0; i < page_count; ++i) {
            VirAddr vaddr  = map_area.begin + i * PAGESIZE;
            auto query_res = _pman.query_page(vaddr);
            if (!query_res.has_value()) {
                if (query_res.error() == ErrCode::PAGE_NOT_PRESENT) {
                    continue;
                }
                propagate_return(query_res);
            }
            auto qres = query_res.value();
            if (qres.size != PageMan::PageSize::_4K) {
                unexpect_return(ErrCode::NOT_SUPPORTED);
            }
            PageMan::RWX rwx = PageMan::rwx(*qres.pte);
            if (PageMan::is_writable(rwx)) {
                qres.pte->rwx = rwx_cast(PageMan::without_write(rwx));
                PageMan::set_cow(qres.pte, true);
            }
        }
    }
    _pman.flush_tlb();
    void_return();
}

void TaskMemoryManager::unmap_memory_tail(cap::MemoryPayload *memory,
                                          size_t new_size) {
    for (auto &vma : vma_list) {
        if (vma.memory != memory) {
            continue;
        }
        size_t keep = new_size > vma.mem_offset ? new_size - vma.mem_offset : 0;
        VirAddr new_end =
            vma.varea.begin + (keep < vma.size() ? keep : vma.size());
        if (new_end < vma.varea.end) {
            unmap_pages(VirArea(new_end, vma.varea.end));
            vma.varea.end = new_end;
        }
    }
}

Result<void> TaskMemoryManager::sync_memory_vmas(cap::MemoryPayload *memory) {
    for (auto &vma : vma_list) {
        if (vma.memory != memory) {
            continue;
        }
        VirArea old_area = vma.varea;
        VirArea new_area = old_area;
        size_t size =
            memory->memsz > vma.mem_offset ? memory->memsz - vma.mem_offset : 0;
        if (vma.growth & VMA::Growth::GROW_DOWN) {
            new_area.begin = old_area.end - size;
        } else {
            new_area.end = old_area.begin + size;
        }
        auto grow_res = grow_vma(util::nnullforce(&vma), new_area);
        propagate(grow_res);
    }
    void_return();
}

Result<cap::MemoryPayload *> TaskMemoryManager::cloned_memory_for(
    cap::MemoryPayload *source, TaskMemoryManager &dst) const {
    for (auto &vma : vma_list) {
        if (vma.memory != source) {
            continue;
        }
        auto child_vma_res = dst.locate_range(vma.varea);
        propagate(child_vma_res);
        return child_vma_res.value()->memory;
    }
    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
}

bool TaskMemoryManager::on_np(const NoPresentEvent &e) {
    loggers::PAGING::DEBUG(
        "TM::on_np: access_address=%p, tm_pgd=%p, pman_root=%p",
        e.access_address.addr(), _pgd.addr(), _pman.get_root().addr());
    auto locate_res = locate(e.access_address);
    if (!locate_res.has_value()) {
        loggers::PAGING::ERROR(
            "TM::on_np: 地址不在任何 VMA 中: addr = %p, err=%s",
            e.access_address.addr(), to_cstring(locate_res.error()));
        return false;
    }
    VMA *vma = locate_res.value();

    VirAddr aligned_vaddr = e.access_address.page_align_down();
    size_t mem_offset     = memory_offset_for_page(*vma, aligned_vaddr);
    auto ensure_res       = vma->memory->ensure_page(mem_offset);
    if (!ensure_res.has_value()) {
        loggers::TASK::ERROR("无法处理缺页异常: err=%d", ensure_res.error());
        return false;
    }
    auto page_res = vma->memory->lookup_page(mem_offset);
    if (!page_res.has_value()) {
        loggers::TASK::ERROR("缺页补页后查询失败: err=%d", page_res.error());
        return false;
    }
    PhyAddr paddr = page_res.value();
    assert(paddr.nonnull());

    // 如果正在加载, 此时应当给予读写权限
    PageMan::RWX rwx = vma->loading ? PageMan::RWX::RW : vma->rwx;
    auto refcount_res = vma->memory->page_refcount(mem_offset);
    if (!refcount_res.has_value()) {
        loggers::PAGING::ERROR(
            "TM::on_np: 无法获取页共享计数: addr=%p err=%d",
            aligned_vaddr.addr(), refcount_res.error());
        return false;
    }
    bool cow_page = !vma->memory->shared && PageMan::is_writable(rwx) &&
                    refcount_res.value() > 1;
    PageMan::RWX map_rwx = cow_page ? PageMan::without_write(rwx) : rwx;
    bool u = !vma->loading;  // 加载过程中按内核页处理, 加载完成后按用户页处理

    _pman.map_page<PageMan::PageSize::_4K>(aligned_vaddr, paddr, map_rwx, u,
                                           false);
    if (cow_page) {
        auto query_res = _pman.query_page(aligned_vaddr);
        if (query_res.has_value()) {
            PageMan::set_cow(query_res.value().pte, true);
        }
    }
    _pman.flush_tlb();
    loggers::PAGING::DEBUG(
        "TM::on_np: mapped addr=%p page=%p cow=%d", e.access_address.addr(),
        aligned_vaddr.addr(), cow_page);

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

bool TaskMemoryManager::on_wp(VirAddr fault_addr) {
    auto locate_res = locate(fault_addr);
    if (!locate_res.has_value()) {
        loggers::PAGING::ERROR("TM::on_wp: 地址不在任何 VMA 中: addr=%p",
                               fault_addr.addr());
        return false;
    }
    VMA *vma = locate_res.value();
    if (vma->memory == nullptr || vma->memory->shared) {
        loggers::PAGING::ERROR("TM::on_wp: VMA 不支持 COW: type=%s addr=%p",
                               to_string(vma->type), fault_addr.addr());
        return false;
    }

    VirAddr aligned_vaddr = fault_addr.page_align_down();
    auto query_res        = _pman.query_page(aligned_vaddr);
    if (!query_res.has_value()) {
        loggers::PAGING::ERROR("TM::on_wp: 查询页表失败: addr=%p err=%d",
                               aligned_vaddr.addr(), query_res.error());
        return false;
    }
    auto qres = query_res.value();
    if (qres.size != PageMan::PageSize::_4K) {
        loggers::PAGING::ERROR("TM::on_wp: COW 暂不支持大页: addr=%p",
                               aligned_vaddr.addr());
        return false;
    }
    if (!PageMan::is_cow(*qres.pte)) {
        loggers::PAGING::ERROR("TM::on_wp: 写保护页不是 COW 页: addr=%p",
                               aligned_vaddr.addr());
        return false;
    }

    size_t mem_offset = memory_offset_for_page(*vma, aligned_vaddr);
    auto fork_res     = vma->memory->fork(mem_offset);
    if (!fork_res.has_value()) {
        loggers::PAGING::ERROR("TM::on_wp: COW fork 失败: addr=%p err=%d",
                               aligned_vaddr.addr(), fork_res.error());
        return false;
    }

    auto page_res = vma->memory->lookup_page(mem_offset);
    if (!page_res.has_value()) {
        loggers::PAGING::ERROR("TM::on_wp: fork 后查询页失败: addr=%p err=%d",
                               aligned_vaddr.addr(), page_res.error());
        return false;
    }

    PageMan::set_paddr(qres.pte, page_res.value());
    qres.pte->rwx = rwx_cast(vma->rwx);
    PageMan::set_cow(qres.pte, false);
    PageMan::flush_tlb();
    loggers::PAGING::INFO("TM::on_wp: resolved cow addr=%p page=%p",
                          fault_addr.addr(), aligned_vaddr.addr());
    return true;
}

Result<void> TaskMemoryManager::clone_to_cow(TaskMemoryManager &dst) {
    for (auto &vma : vma_list) {
        auto *memory =
            static_cast<cap::MemoryPayload *>(vma.memory->clone_payload());
        auto add_res = dst.add_vma(vma.type, vma.growth, vma.varea, memory,
                                   vma.rwx, vma.mem_offset);
        propagate(add_res);

        VirArea map_area = page_outer_area(vma.varea);
        if (map_area.nullable()) {
            continue;
        }
        auto clone_res = clone_vma_pages_to_cow(vma, map_area, dst);
        propagate(clone_res);
    }
    _pman.flush_tlb();
    dst.pman().flush_tlb();
    void_return();
}

Result<void> TaskMemoryManager::clone_vma_pages_to_cow(const VMA &vma,
                                                       const VirArea &map_area,
                                                       TaskMemoryManager &dst) {
    size_t page_count = map_area.size() / PAGESIZE;
    for (size_t i = 0; i < page_count; ++i) {
        VirAddr vaddr  = map_area.begin + i * PAGESIZE;
        auto query_res = _pman.query_page(vaddr);
        if (!query_res.has_value()) {
            if (query_res.error() == ErrCode::PAGE_NOT_PRESENT) {
                continue;
            }
            propagate_return(query_res);
        }

        auto qres = query_res.value();
        if (qres.size != PageMan::PageSize::_4K) {
            loggers::PAGING::ERROR("clone_to_cow: COW 暂不支持大页: addr=%p",
                                   vaddr.addr());
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        PhyAddr paddr    = PageMan::get_physical_address(*qres.pte);
        PageMan::RWX rwx = PageMan::rwx(*qres.pte);
        bool cow_page    = !vma.memory->shared && (PageMan::is_writable(rwx) ||
                                                PageMan::is_cow(*qres.pte));
        PageMan::RWX child_rwx = cow_page ? PageMan::without_write(rwx) : rwx;

        dst.pman().map_page<PageMan::PageSize::_4K>(
            vaddr, paddr, child_rwx, PageMan::is_user_accessible(*qres.pte),
            PageMan::is_global(*qres.pte));

        if (cow_page) {
            qres.pte->rwx = rwx_cast(PageMan::without_write(rwx));
            PageMan::set_cow(qres.pte, true);

            auto dst_query_res = dst.pman().query_page(vaddr);
            if (!dst_query_res.has_value()) {
                propagate_return(dst_query_res);
            }
            PageMan::set_cow(dst_query_res.value().pte, true);
        }
    }
    void_return();
}
