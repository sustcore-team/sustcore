/**
 * @file memory.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Memory capability object
 * @version alpha-1.0.0
 * @date 2026-05-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/gfp.h>
#include <mem/vma.h>
#include <object/memory.h>
#include <object/perm.h>
#include <object/vfile.h>
#include <logger.h>
#include <sustcore/errcode.h>

#include <cstring>

namespace {
    /**
     * @brief 将 Memory 内偏移向下对齐到页边界. 
     */
    [[nodiscard]]
    size_t offset_to_offvpn(size_t offset) noexcept {
        return page_align_down(offset) / PAGESIZE;
    }

    /**
     * @brief 将 offvpn 转为 payload 内页对齐字节偏移. 
     */
    [[nodiscard]]
    size_t offvpn_to_offset(size_t offvpn) noexcept {
        return offvpn * PAGESIZE;
    }

    /**
     * @brief 获取指定偏移所在页内偏移. 
     */
    [[nodiscard]]
    size_t offset_in_page(size_t offset) noexcept {
        return offset % PAGESIZE;
    }

    /**
     * @brief 计算以当前偏移为起点, 在当前页内最多可访问的字节数. 
     */
    [[nodiscard]]
    size_t page_chunk_size(size_t offset, size_t remain) noexcept {
        size_t capacity = PAGESIZE - offset_in_page(offset);
        return capacity < remain ? capacity : remain;
    }

    /**
     * @brief 查询指定 offvpn 对应页记录. 
     */
    [[nodiscard]]
    Result<std::reference_wrapper<cap::PhyPage>> lookup_page_entry(
        cap::MemoryPayload &memory, size_t offvpn) noexcept {
        auto it = memory.phy_pages.find(offvpn);
        if (it == memory.phy_pages.end()) {
            unexpect_return(ErrCode::PAGE_NOT_PRESENT);
        }
        return std::ref(it->second);
    }

    /**
     * @brief 查询指定 offvpn 对应只读页记录. 
     */
    [[nodiscard]]
    Result<std::reference_wrapper<const cap::PhyPage>> lookup_page_entry(
        const cap::MemoryPayload &memory, size_t offvpn) noexcept {
        auto it = memory.phy_pages.find(offvpn);
        if (it == memory.phy_pages.end()) {
            unexpect_return(ErrCode::PAGE_NOT_PRESENT);
        }
        return std::cref(it->second);
    }

    /**
     * @brief 判断请求的增长/收缩方式是否被 Memory 属性允许. 
     */
    [[nodiscard]]
    bool growth_allows(cap::MemoryGrowth owned,
                       cap::MemoryGrowth requested) noexcept {
        if (requested == cap::MemoryGrowth::FIXED) {
            return owned == cap::MemoryGrowth::FIXED;
        }
        return (static_cast<b64>(requested) & ~static_cast<b64>(owned)) == 0;
    }

    /**
     * @brief 为 continuity Memory 预分配连续物理页. 
     *
     * 已分配过物理页或大小为 0 时不做任何操作. 
     */
    [[nodiscard]]
    Result<void> ensure_contiguous(cap::MemoryPayload *memory) {
        if (memory == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (!memory->continuity || memory->allocated_size() != 0) {
            void_return();
        }
        size_t pages = page_align_up(memory->memsz) / PAGESIZE;
        if (pages == 0) {
            void_return();
        }
        auto paddr_res = GFP::get_free_page(pages);
        propagate(paddr_res);
        PhyAddr base = paddr_res.value();
        for (size_t i = 0; i < pages; ++i) {
            memory->phy_pages.insert_or_assign(i,
                                               cap::PhyPage{.addr=base + i * PAGESIZE,
                                                            .refcount=1});
        }
        void_return();
    }

    /**
     * @brief 计算从指定偏移起, 当前 payload 中仍可访问的总长度. 
     */
    [[nodiscard]]
    Result<size_t> bounded_length(size_t memsz, size_t offset,
                                  size_t buflen) noexcept {
        if (offset >= memsz) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        size_t available = memsz - offset;
        return available < buflen ? available : buflen;
    }
}  // namespace

namespace cap {
    MemoryPayload::MemoryPayload(size_t memsz, bool shared, bool continuity,
                                 MemoryGrowth growth,
                                 util::owner<Capability *> file,
                                 size_t file_offset)
        : memsz(memsz),
          shared(shared),
          continuity(continuity),
          growth(growth),
          file(file),
          file_offset(file_offset),
          phy_pages() {}

    MemoryPayload::~MemoryPayload() {
        delete file.get();
        file = util::owner<Capability *>(nullptr);
    }

    void MemoryPayload::destruct() {
        for (auto &page : phy_pages) {
            GFP::put_page(page.second.addr, 1);
        }
        delete this;
    }

    Payload *MemoryPayload::clone_payload() {
        if (shared) {
            return this;
        }

        auto cloned_file = file_backed()
                               ? util::owner<Capability *>(file->clone())
                               : util::owner<Capability *>(nullptr);
        auto *cloned = new MemoryPayload(memsz, shared, continuity, growth,
                                         cloned_file, file_offset);
        for (auto &page : phy_pages) {
            GFP::keep_page(page.second.addr, 1);
            page.second.refcount++;
            cloned->phy_pages.insert_or_assign(page.first, page.second);
        }
        return cloned;
    }

    Result<PhyAddr> MemoryPayload::lookup_page(size_t offset) const noexcept {
        if (page_align_down(offset) >= memsz) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        return lookup_page_entry(*this, offset_to_offvpn(offset))
            .transform(std::mem_fn(&PhyPage::addr));
    }

    Result<PhyAddr> MemoryPayload::ensure_page(size_t offset) {
        auto lookup_res = lookup_page(offset);
        if (lookup_res.has_value()) {
            return lookup_res.value();
        }
        if (lookup_res.error() != ErrCode::PAGE_NOT_PRESENT) {
            propagate_return(lookup_res);
        }

        size_t offvpn = offset_to_offvpn(offset);
        auto page_res = GFP::get_free_page(1);
        propagate(page_res);
        PhyAddr paddr = page_res.value();
        memset(convert<KpaAddr>(paddr).addr(), 0, PAGESIZE);
        if (file_backed()) {
            cap::VFileObject file_obj(util::nnullforce(file.get()));
            size_t page_file_offset = offvpn_to_offset(offvpn);
            if (page_file_offset > static_cast<size_t>(-1) - file_offset) {
                GFP::put_page(paddr, 1);
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            // Short reads leave the remaining zero-filled bytes in place, which
            // matches file-backed mmap semantics for pages extending past EOF.
            auto read_res = file_obj.read(file_offset + page_file_offset,
                                          convert<KpaAddr>(paddr).addr(),
                                          PAGESIZE);
            if (!read_res.has_value()) {
                GFP::put_page(paddr, 1);
                propagate_return(read_res);
            }
        }
        phy_pages.insert_or_assign(offvpn, PhyPage{paddr, 1});
        loggers::PAGING::DEBUG(
            "MemoryPayload::ensure_page: mem=%p offvpn=%lu paddr=%p", this,
            offvpn, paddr.addr());
        return paddr;
    }

    Result<size_t> MemoryPayload::page_refcount(size_t offset) const noexcept {
        if (page_align_down(offset) >= memsz) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        return lookup_page_entry(*this, offset_to_offvpn(offset))
            .transform(std::mem_fn(&PhyPage::refcount));
    }

    Result<void> MemoryPayload::replace_page(size_t offset,
                                             PhyAddr new_addr) noexcept {
        size_t offvpn = offset_to_offvpn(offset);
        auto entry_res = lookup_page_entry(*this, offvpn);
        propagate(entry_res);

        auto &page   = entry_res.value().get();
        PhyAddr old  = page.addr;
        page.addr    = new_addr;
        page.refcount = 1;
        GFP::put_page(old, 1);
        loggers::PAGING::DEBUG(
            "MemoryPayload::replace_page: offvpn=%lu old=%p new=%p", offvpn,
            old.addr(), new_addr.addr());
        void_return();
    }

    Result<size_t> MemoryPayload::read(size_t offset, void *data, size_t buflen) {
        if (data == nullptr && buflen != 0) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (buflen == 0) {
            return size_t{0};
        }

        auto total_res = bounded_length(memsz, offset, buflen);
        propagate(total_res);
        size_t total = total_res.value();

        auto *dst       = static_cast<char *>(data);
        size_t consumed = 0;
        while (consumed < total) {
            size_t cur_offset = offset + consumed;
            size_t chunk      = page_chunk_size(cur_offset, total - consumed);
            auto page_res     = ensure_page(cur_offset);
            propagate(page_res);
            PhyAddr paddr = page_res.value();
            memcpy(dst + consumed,
                   static_cast<char *>(convert<KpaAddr>(paddr).addr()) +
                       offset_in_page(cur_offset),
                   chunk);
            consumed += chunk;
        }

        loggers::PAGING::DEBUG(
            "MemoryPayload::read: offset=%lu len=%lu actual=%lu", offset,
            buflen, total);
        return total;
    }

    Result<size_t> MemoryPayload::write(size_t offset, const void *data,
                                        size_t buflen) {
        if (data == nullptr && buflen != 0) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (buflen == 0) {
            return size_t{0};
        }

        auto total_res = bounded_length(memsz, offset, buflen);
        propagate(total_res);
        size_t total = total_res.value();

        auto *src       = static_cast<const char *>(data);
        size_t consumed = 0;
        while (consumed < total) {
            size_t cur_offset = offset + consumed;
            size_t chunk      = page_chunk_size(cur_offset, total - consumed);
            auto page_res     = ensure_page(cur_offset);
            propagate(page_res);

            auto refcount_res = page_refcount(cur_offset);
            propagate(refcount_res);
            if (refcount_res.value() > 1) {
                auto fork_res = fork(cur_offset);
                propagate(fork_res);
            }

            auto current_res = lookup_page(cur_offset);
            propagate(current_res);
            PhyAddr paddr = current_res.value();
            memcpy(static_cast<char *>(convert<KpaAddr>(paddr).addr()) +
                       offset_in_page(cur_offset),
                   src + consumed, chunk);
            consumed += chunk;
        }

        loggers::PAGING::DEBUG(
            "MemoryPayload::write: offset=%lu len=%lu actual=%lu", offset,
            buflen, total);
        return total;
    }

    Result<void> MemoryPayload::fork(size_t offset) {
        if (page_align_down(offset) >= memsz) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        size_t offvpn       = offset_to_offvpn(offset);
        auto entry_res      = lookup_page_entry(*this, offvpn);
        propagate(entry_res);
        auto &page          = entry_res.value().get();
        size_t old_refcount = page.refcount;
        if (old_refcount <= 1) {
            page.refcount = 1;
            loggers::PAGING::DEBUG(
                "MemoryPayload::fork: offvpn=%lu already exclusive paddr=%p",
                offvpn, page.addr.addr());
            void_return();
        }

        auto new_page_res = GFP::get_free_page(1);
        propagate(new_page_res);
        PhyAddr new_paddr = new_page_res.value();
        memcpy(convert<KpaAddr>(new_paddr).addr(),
               convert<KpaAddr>(page.addr).addr(), PAGESIZE);

        PhyAddr old_paddr = page.addr;
        page.addr         = new_paddr;
        page.refcount     = 1;
        old_refcount--;
        GFP::put_page(old_paddr, 1);
        loggers::PAGING::DEBUG(
            "MemoryPayload::fork: offvpn=%lu old=%p new=%p shared_ref=%lu",
            offvpn, old_paddr.addr(), new_paddr.addr(), old_refcount);
        void_return();
    }

    void MemoryPayload::release_pages_from(size_t offset) noexcept {
        size_t first_offvpn = page_align_up(offset) / PAGESIZE;
        for (auto it = phy_pages.begin(); it != phy_pages.end();) {
            if (it->first >= first_offvpn) {
                GFP::put_page(it->second.addr, 1);
                it = phy_pages.erase(it);
            } else {
                ++it;
            }
        }
    }

    Result<void> MemoryPayload::resize(size_t newsz) {
        if (newsz == memsz) {
            void_return();
        }
        if (shared) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        bool grow   = newsz > memsz && ((growth & MemoryGrowth::GROW_UP) ||
                                      (growth & MemoryGrowth::GROW_DOWN));
        bool shrink = newsz < memsz && ((growth & MemoryGrowth::SHRINK_UP) ||
                                        (growth & MemoryGrowth::SHRINK_DOWN));
        if (!grow && !shrink) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        if (continuity) {
            size_t old_pages = page_align_up(memsz) / PAGESIZE;
            size_t new_pages = page_align_up(newsz) / PAGESIZE;
            if (new_pages == 0) {
                release_pages_from(0);
                memsz = newsz;
                void_return();
            }

            auto new_base_res = GFP::get_free_page(new_pages);
            propagate(new_base_res);
            PhyAddr new_base = new_base_res.value();
            memset(convert<KpaAddr>(new_base).addr(), 0, new_pages * PAGESIZE);

            size_t copy_pages = old_pages < new_pages ? old_pages : new_pages;
            for (size_t i = 0; i < copy_pages; ++i) {
                auto old_page_res = lookup_page(offvpn_to_offset(i));
                if (old_page_res.has_value()) {
                    memcpy(convert<KpaAddr>(new_base + i * PAGESIZE).addr(),
                           convert<KpaAddr>(old_page_res.value()).addr(),
                           PAGESIZE);
                }
            }

            for (auto &page : phy_pages) {
                GFP::put_page(page.second.addr, 1);
            }
            phy_pages.clear();
            for (size_t i = 0; i < new_pages; ++i) {
                phy_pages.insert_or_assign(i,
                                           PhyPage{new_base + i * PAGESIZE, 1});
            }
        } else if (newsz < memsz) {
            release_pages_from(newsz);
        }
        memsz = newsz;
        void_return();
    }

    size_t MemoryPayload::allocated_size() const noexcept {
        return phy_pages.size() * PAGESIZE;
    }

    Result<void> MemoryObject::map_into(TaskMemoryManager &tmm, VirAddr vaddr,
                                        PageMan::RWX rwx,
                                        MemoryGrowth req_growth) const {
        if (!imply(perm::memory::MAP)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (!PageMan::is_readable(rwx)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (!imply(perm::memory::READ)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (PageMan::is_writable(rwx) && !imply(perm::memory::WRITE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (PageMan::is_executable(rwx) && !imply(perm::memory::EXEC)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if ((req_growth & MemoryGrowth::GROW_UP ||
             req_growth & MemoryGrowth::SHRINK_UP) &&
            !imply(perm::memory::FLEXUP))
        {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if ((req_growth & MemoryGrowth::GROW_DOWN ||
             req_growth & MemoryGrowth::SHRINK_DOWN) &&
            !imply(perm::memory::FLEXDOWN))
        {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (!growth_allows(_obj->growth, req_growth)) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        auto cont_res = ensure_contiguous(_obj);
        propagate(cont_res);

        VirArea area(vaddr, vaddr + _obj->memsz);
        auto add_res = tmm.add_vma(
            VMA::Type::SHARE, req_growth, area, _obj,
            VMA::rwx_to_prot(rwx, true));
        propagate(add_res);
        void_return();
    }

    Result<void> MemoryObject::unmap_from(TaskMemoryManager &tmm,
                                          VirAddr vaddr) const {
        if (!imply(perm::memory::MAP)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        auto locate_res = tmm.locate_memory(_obj, vaddr);
        propagate(locate_res);
        return tmm.remove_vma(locate_res.value());
    }

    Result<void> MemoryObject::resize_in(TaskMemoryManager *tmm,
                                         size_t newsz) const {
        if (!imply(perm::memory::RESIZE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (tmm != nullptr && newsz < _obj->memsz) {
            tmm->unmap_memory_tail(_obj, newsz);
        }
        auto resize_res = _obj->resize(newsz);
        propagate(resize_res);
        if (tmm != nullptr) {
            auto sync_res = tmm->sync_memory_vmas(_obj);
            propagate(sync_res);
        }
        void_return();
    }

    Result<MemoryObject::QueryResult> MemoryObject::query() const {
        if (!imply(perm::memory::QUERY)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        return QueryResult{_obj->memsz, _obj->allocated_size()};
    }
}  // namespace cap
