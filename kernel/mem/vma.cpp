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

#include <env.h>
#include <mem/gfp.h>
#include <mem/vma.h>
#include <sus/logger.h>
#include <sus/owner.h>
#include <sus/range.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>

#include <cstring>

namespace {
    [[nodiscard]]
    bool less_by_begin(const VMA &lhs, const VMA &rhs) noexcept {
        return lhs.varea.begin < rhs.varea.begin;
    }

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
    auto kernel_pgd = env::inst().main_kernel_pgd();
    assert(kernel_pgd.nonnull());
    PageMan kernel_pman(kernel_pgd);
    auto merge_res = _pman.merge_from(kernel_pman);
    assert(merge_res.has_value());
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
    cap::MemoryPayload *memory, VMA::Prot prot, size_t mem_offset) {
    if (memory == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }
    if (!valid_user_area(varea)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    if (!varea.nullable() && locate_range(varea).has_value()) {
        unexpect_return(ErrCode::BUSY);
    }

    VMA *vma = new VMA(this, type, growth, varea, memory, prot, mem_offset);
    auto it  = vma_list.begin();
    for (; it != vma_list.end(); ++it) {
        if (varea.begin < it->varea.begin) {
            break;
        }
    }
    if (it == vma_list.end()) {
        vma_list.push_back(*vma);
    } else {
        vma_list.insert(it, *vma);
    }
    return util::nonnull(*vma);
}

Result<util::nonnull<VMA *>> TaskMemoryManager::clone_vma(
    util::nonnull<VMA *> vma, TaskMemoryManager &dst) {
    return __check_vma(vma).and_then([&dst](VMA *vma) {
        return dst.add_vma(vma->type, vma->growth, vma->varea,
                           vma->memory_payload(), vma->prot,
                           vma->mem_offset);
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
        if (vma.memory_payload() == memory &&
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

Result<void> TaskMemoryManager::remove_vma_range(const VirArea &varea) {
    if (varea.nullable()) {
        void_return();
    }
    if (!valid_user_area(varea)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto it = vma_list.begin();
    while (it != vma_list.end()) {
        VMA *vma = &*it;
        ++it;
        if (!is_intersecting(vma->varea, varea)) {
            continue;
        }

        VirAddr cut_begin = varea.begin > vma->varea.begin ? varea.begin
                                                           : vma->varea.begin;
        VirAddr cut_end   = varea.end < vma->varea.end ? varea.end
                                                       : vma->varea.end;
        if (cut_begin >= cut_end) {
            continue;
        }

        VirArea old_area = vma->varea;
        const size_t cut_offset = static_cast<size_t>(cut_begin - old_area.begin);
        const size_t cut_size   = static_cast<size_t>(cut_end - cut_begin);

        if (cut_begin == old_area.begin && cut_end == old_area.end) {
            auto remove_res = remove_vma(util::nnullforce(vma));
            propagate(remove_res);
            continue;
        }
        if (cut_begin == old_area.begin) {
            unmap_pages(VirArea(cut_begin, cut_end));
            vma->varea.begin = cut_end;
            vma->mem_offset += cut_size;
            continue;
        }
        if (cut_end == old_area.end) {
            unmap_pages(VirArea(cut_begin, cut_end));
            vma->varea.end = cut_begin;
            continue;
        }

        VMA right(this, vma->type, vma->growth, VirArea(cut_end, old_area.end),
                  vma->memory_payload(), vma->prot,
                  vma->mem_offset + cut_offset + cut_size);
        unmap_pages(VirArea(cut_begin, cut_end));
        vma->varea.end = cut_begin;
        auto add_res = add_vma(right.type, right.growth, right.varea,
                               right.memory_payload(), right.prot,
                               right.mem_offset);
        propagate(add_res);
    }
    _pman.flush_tlb();
    void_return();
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
        if (vma.memory_payload() == memory) {
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
        if (vma.memory_payload() != memory) {
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
                PageMan::protect_cow(qres.pte, rwx);
            }
        }
    }
    _pman.flush_tlb();
    void_return();
}

void TaskMemoryManager::unmap_memory_tail(cap::MemoryPayload *memory,
                                          size_t new_size) {
    for (auto &vma : vma_list) {
        if (vma.memory_payload() != memory) {
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
        if (vma.memory_payload() != memory) {
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
        if (vma.memory_payload() != source) {
            continue;
        }
        auto child_vma_res = dst.locate_range(vma.varea);
        propagate(child_vma_res);
        return child_vma_res.value()->memory_payload();
    }
    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
}

Result<TaskMemoryManager::VMAQueryResult> TaskMemoryManager::query_vaddr(
    VirAddr vaddr, CapIdx mem_cap) const {
    for (const auto &vma : vma_list) {
        if (within(vma.varea, vaddr) ||
            (vma.varea.size() == 0 && vma.varea.begin == vaddr))
        {
            return VMAQueryResult{
                .type    = vma.type,
                .prot    = vma.prot,
                .start   = vma.varea.begin,
                .size    = vma.size(),
                .mem_cap = mem_cap,
            };
        }
    }
    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
}

std::vector<TaskMemoryManager::VMAQueryResult> TaskMemoryManager::query_vspace(
    size_t offset, size_t max_entries,
    const std::function<CapIdx(const VMA &)> &mem_cap_resolver) const {
    std::vector<VMAQueryResult> results{};
    if (max_entries == 0) {
        return results;
    }

    size_t index = 0;
    for (const auto &vma : vma_list) {
        if (index++ < offset) {
            continue;
        }
        if (results.size() >= max_entries) {
            break;
        }
        results.push_back(VMAQueryResult{
            .type    = vma.type,
            .prot    = vma.prot,
            .start   = vma.varea.begin,
            .size    = vma.size(),
            .mem_cap = mem_cap_resolver(vma),
        });
    }
    return results;
}

bool TaskMemoryManager::on_np(const NoPresentEvent &e) {
    loggers::PAGING::DEBUG(
        "TM::on_np: access_address=%p, tm_pgd=%p, pman_root=%p",
        e.access_address.addr(), _pgd.addr(), _pman.get_root().addr());

    // locate the vma
    auto locate_res = locate(e.access_address);
    if (!locate_res.has_value()) {
        loggers::PAGING::ERROR(
            "TM::on_np: 地址不在任何 VMA 中: addr = %p, err=%s",
            e.access_address.addr(), to_cstring(locate_res.error()));
        return false;
    }
    VMA *vma = locate_res.value();
    auto *memory = vma->memory_payload();
    if (memory == nullptr) {
        loggers::PAGING::ERROR("TM::on_np: VMA 无有效 Memory: addr=%p",
                               e.access_address.addr());
        return false;
    }

    // locate the page in memory
    VirAddr aligned_vaddr = e.access_address.page_align_down();
    size_t mem_offset     = memory_offset_for_page(*vma, aligned_vaddr);
    // 保证这个页在 vma->memory 中存在
    auto ensure_res       = memory->ensure_page(mem_offset);
    if (!ensure_res.has_value()) {
        loggers::TASK::ERROR("无法处理缺页异常: err=%d", ensure_res.error());
        return false;
    }
    auto page_res = memory->lookup_page(mem_offset);
    if (!page_res.has_value()) {
        loggers::TASK::ERROR("缺页补页后查询失败: err=%d", page_res.error());
        return false;
    }
    PhyAddr paddr = page_res.value();
    assert(paddr.nonnull());

    // load the page
    PageMan::RWX rwx  = VMA::prot_to_rwx(vma->prot);
    auto refcount_res = memory->page_refcount(mem_offset);
    if (!refcount_res.has_value()) {
        loggers::PAGING::ERROR("TM::on_np: 无法获取页共享计数: addr=%p err=%d",
                               aligned_vaddr.addr(), refcount_res.error());
        return false;
    }
    // 是否需要 cow 标记
    bool cow_page = !memory->shared && PageMan::is_writable(rwx) && refcount_res.value() > 1;
    PageMan::RWX map_rwx = cow_page ? PageMan::without_write(rwx) : rwx;

    // 映射该页
    _pman.map_page<PageMan::PageSize::_4K>(
        aligned_vaddr, paddr, PageMan::page_flags(map_rwx, true, false));
    if (cow_page) {
        // cow 需要 cow 标记
        auto query_res = _pman.query_page(aligned_vaddr);
        if (query_res.has_value()) {
            PageMan::protect_cow(query_res.value().pte, rwx);
        }
    }
    // 刷新 tlb
    _pman.flush_tlb();
    loggers::PAGING::DEBUG("TM::on_np: mapped addr=%p page=%p cow=%d",
                           e.access_address.addr(), aligned_vaddr.addr(),
                           cow_page);

    // 调试: 使用当前硬件页表根再次查询该页
    // PhyAddr hw_root = PageMan::read_root();
    // PageMan verify_pman(hw_root);
    // auto verify_res = verify_pman.query_page(aligned_vaddr);
    // if (!verify_res.has_value()) {
    //     loggers::PAGING::ERROR(
    //         "TM::on_np: 映射后在当前页表中仍查不到该页: vaddr=%p, "
    //         "err=%d, hw_root=%p, tm_pgd=%p",
    //         aligned_vaddr.addr(), verify_res.error(), hw_root.addr(),
    //         _pgd.addr());
    // } else {
    //     loggers::PAGING::DEBUG(
    //         "TM::on_np: 页映射成功: vaddr=%p, hw_root=%p, tm_pgd=%p",
    //         aligned_vaddr.addr(), hw_root.addr(), _pgd.addr());
    // }
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
    auto *memory = vma->memory_payload();
    if (memory == nullptr || memory->shared) {
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
    auto fork_res     = memory->fork(mem_offset);
    if (!fork_res.has_value()) {
        loggers::PAGING::ERROR("TM::on_wp: COW fork 失败: addr=%p err=%d",
                               aligned_vaddr.addr(), fork_res.error());
        return false;
    }

    auto page_res = memory->lookup_page(mem_offset);
    if (!page_res.has_value()) {
        loggers::PAGING::ERROR("TM::on_wp: fork 后查询页失败: addr=%p err=%d",
                               aligned_vaddr.addr(), page_res.error());
        return false;
    }

    PageMan::set_paddr(qres.pte, page_res.value());
    PageMan::restore_from_cow(
        qres.pte,
        PageMan::page_flags(VMA::prot_to_rwx(vma->prot),
                            PageMan::is_user_accessible(*qres.pte),
                            PageMan::is_global(*qres.pte),
                            PageMan::is_present(*qres.pte)));
    PageMan::flush_tlb();
    loggers::PAGING::DEBUG("TM::on_wp: resolved cow addr=%p page=%p",
                          fault_addr.addr(), aligned_vaddr.addr());
    return true;
}

Result<void> TaskMemoryManager::clone_to_cow(TaskMemoryManager &dst) {
    for (auto &vma : vma_list) {
        auto *source = vma.memory_payload();
        if (source == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *memory = static_cast<cap::MemoryPayload *>(source->clone_payload());
        auto add_res = dst.add_vma(vma.type, vma.growth, vma.varea, memory,
                                   vma.prot, vma.mem_offset);
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
        auto *memory     = vma.memory_payload();
        if (memory == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        bool cow_page    = !memory->shared && (PageMan::is_writable(rwx) ||
                                             PageMan::is_cow(*qres.pte));
        PageMan::RWX child_rwx = cow_page ? PageMan::without_write(rwx) : rwx;

        dst.pman().map_page<PageMan::PageSize::_4K>(
            vaddr, paddr,
            PageMan::page_flags(
                child_rwx, PageMan::is_user_accessible(*qres.pte),
                PageMan::is_global(*qres.pte), PageMan::is_present(*qres.pte)));

        if (cow_page) {
            PageMan::protect_cow(qres.pte, rwx);

            auto dst_query_res = dst.pman().query_page(vaddr);
            if (!dst_query_res.has_value()) {
                propagate_return(dst_query_res);
            }
            PageMan::protect_cow(dst_query_res.value().pte, rwx);
        }
    }
    void_return();
}
