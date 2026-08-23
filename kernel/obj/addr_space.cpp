/**
 * @file addr_space.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief AddrSpace VMA 管理、缺页映射和页表生命周期。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/addr_space.h>
#include <tay/lock.h>

#include <new>
#include <utility>

namespace task {
    namespace {
        struct FaultSnapshot final {
            cap::CRef<memory::MemSeg> memory{};
            VirArea area{};
            size_t seg_offset = 0;
            memory::PageFlags flags{};
            u64_t generation = 0;
        };

        [[nodiscard]] bool valid_flags(memory::PageFlags flags) noexcept {
            return flags.readable && !(flags.writable && flags.executable);
        }

        [[nodiscard]] bool allows(memory::PageFlags flags, memory::FaultAccess access) noexcept {
            switch (access) {
                case memory::FaultAccess::READ:    return flags.readable;
                case memory::FaultAccess::WRITE:   return flags.writable;
                case memory::FaultAccess::EXECUTE: return flags.executable;
                case memory::FaultAccess::NONE:    return false;
            }
            return false;
        }

        [[nodiscard]] memory::PageFlags seg_flags(u64_t rights) noexcept {
            return memory::PageFlags{
                .readable   = (rights & cap::RIGHT_READ) != 0,
                .writable   = (rights & cap::RIGHT_WRITE) != 0,
                .executable = false,
                .user       = true,
                .global     = false,
            };
        }

        [[nodiscard]] AddrSpaceError page_table_error(VirAddr page,
                                                      const memory::PagingError &error) noexcept {
            return AddrSpaceError::PageTableFailed(page, error.code());
        }
    }  // namespace

    tay::expected<cap::KObjectRef<AddrSpace>, AddrSpaceError> AddrSpace::create() noexcept {
        auto client = tay::create_unique<memory::UserVm, memory::PagingError>();
        if (!client)
            return tay::Err(client.error().code() == kernel::KernelError::PagingError::OUT_OF_MEMORY
                                ? AddrSpaceError::OutOfMemory()
                                : page_table_error({}, client.error()));
        auto *object = new (std::nothrow) AddrSpace(std::move(*client));
        if (object == nullptr)
            return tay::Err(AddrSpaceError::OutOfMemory());
        return cap::KObjectRef<AddrSpace>(*object);
    }

    AddrSpace::~AddrSpace() noexcept {
        auto state   = state_.lock();
        state->state = AddrSpaceState::RETIRING;
        while (!state->vmas.empty()) {
            auto *vma       = state->vmas.pop_front();
            const auto area = vma->area();
            static_cast<void>(unmap_vma_locked(area));
            delete vma;
        }
    }

    tay::expected<VMA *, AddrSpaceError> AddrSpace::add_vma(
        const cap::CRef<memory::MemSeg> &segment, VirArea area, size_t seg_offset,
        memory::PageFlags flags) noexcept {
        if (!segment || segment.object() == nullptr)
            return tay::Err(AddrSpaceError::InvalidSegment());
        if (area.nullable() || !area.begin.aligned<PAGE_SIZE>() || !area.end.aligned<PAGE_SIZE>() ||
            !is_user_vaddr(area.begin) || !is_user_vaddr(area.end - 1))
            return tay::Err(AddrSpaceError::InvalidArea(area));
        if (!valid_flags(flags))
            return tay::Err(AddrSpaceError::InvalidFlags(flags));
        const auto allowed = seg_flags(segment.rights());
        if ((flags.writable && !allowed.writable) || (flags.readable && !allowed.readable))
            return tay::Err(AddrSpaceError::AccessDenied(
                flags.writable ? memory::FaultAccess::WRITE : memory::FaultAccess::READ, allowed));
        const size_t segment_size = segment.object()->size();
        if (seg_offset > segment_size)
            return tay::Err(AddrSpaceError::SegOffsetOutOfRange(seg_offset, segment_size));
        if (area.size() > segment_size - seg_offset)
            return tay::Err(
                AddrSpaceError::MappingExceedsSegment(seg_offset, area.size(), segment_size));
        tay::unique_ptr<VMA> vma(new (std::nothrow) VMA(segment, area, seg_offset, flags));
        if (!vma)
            return tay::Err(AddrSpaceError::OutOfMemory());

        auto state           = state_.lock();
        const auto insertion = find_insert_locked(*state, area);
        if (insertion.overlap != nullptr)
            return tay::Err(AddrSpaceError::VmaOverlap(area, insertion.overlap->area()));
        auto *published = vma.release();
        state->vmas.insert(insertion.before, published);
        ++state->generation;
        state->state = AddrSpaceState::CONFIGURING;
        return published;
    }

    tay::expected<void, AddrSpaceError> AddrSpace::remove_vma(VMA &vma) noexcept {
        auto state = state_.lock();
        bool owned = false;
        for (auto iterator = state->vmas.begin(); iterator != state->vmas.end(); ++iterator) {
            if (*iterator != &vma)
                continue;
            owned = true;
            break;
        }
        if (!owned)
            return tay::Err(AddrSpaceError::VmaNotOwned());
        TAY_TRYV(unmap_vma_locked(vma.area()));
        (void)state->vmas.remove(&vma);
        ++state->generation;
        delete &vma;
        return {};
    }

    VMA *AddrSpace::find_vma_locked(State &state, VirAddr address) noexcept {
        return const_cast<VMA *>(find_vma_locked(static_cast<const State &>(state), address));
    }

    const VMA *AddrSpace::find_vma_locked(const State &state, VirAddr address) noexcept {
        for (auto iterator = state.vmas.begin(); iterator != state.vmas.end(); ++iterator)
            if (tay::within((*iterator)->area(), address))
                return *iterator;
        return nullptr;
    }

    AddrSpace::VmaInsert AddrSpace::find_insert_locked(State &state, VirArea area) noexcept {
        for (auto iterator = state.vmas.begin(); iterator != state.vmas.end(); ++iterator) {
            if (tay::is_intersecting((*iterator)->area(), area))
                return VmaInsert{.before = iterator, .overlap = *iterator};
            if ((*iterator)->area().begin >= area.begin)
                return VmaInsert{.before = iterator};
        }
        return VmaInsert{.before = state.vmas.end()};
    }

    tay::expected<void, AddrSpaceError> AddrSpace::unmap_vma_locked(VirArea area) noexcept {
        for (VirAddr address = area.begin; address < area.end; address += PAGE_SIZE) {
            auto mapping = space_->query(address);
            if (!mapping)
                continue;
            auto unmapped = space_->unmap(address, PAGE_SIZE);
            if (!unmapped)
                return tay::Err(page_table_error(address, unmapped.error()));
        }
        return {};
    }

    tay::expected<memory::PageMapping, AddrSpaceError> AddrSpace::query(
        VirAddr address) const noexcept {
        auto state = state_.lock();
        static_cast<void>(state);
        auto result = space_->query(address);
        if (!result)
            return tay::Err(page_table_error(address, result.error()));
        return *result;
    }

    tay::expected<void, AddrSpaceError> AddrSpace::handle_fault(
        VirAddr address, memory::FaultAccess access) noexcept {
        const VirAddr page = address.page_align_down();
        FaultSnapshot snapshot{};
        {
            auto state = state_.lock();
            auto *vma  = find_vma_locked(*state, address);
            if (vma == nullptr)
                return tay::Err(AddrSpaceError::UnmappedAddress(address));
            if (!allows(vma->flags(), access))
                return tay::Err(AddrSpaceError::AccessDenied(access, vma->flags()));
            auto existing = space_->query(page);
            if (existing)
                return {};
            if (!existing.error().is<memory::PagingError::MissingMapping>())
                return tay::Err(page_table_error(page, existing.error()));
            snapshot = FaultSnapshot{
                .memory     = vma->memory(),
                .area       = vma->area(),
                .seg_offset = vma->seg_offset(),
                .flags      = vma->flags(),
                .generation = state->generation,
            };
        }

        const size_t offset = snapshot.seg_offset + (page - snapshot.area.begin);
        auto physical       = snapshot.memory.object()->ensure_page(offset);
        if (!physical)
            return tay::Err(
                AddrSpaceError::BackingAllocationFailed(offset, physical.error().code()));

        auto state    = state_.lock();
        auto *current = find_vma_locked(*state, address);
        if (state->generation != snapshot.generation || current == nullptr ||
            current->area() != snapshot.area || current->seg_offset() != snapshot.seg_offset ||
            current->memory().object() != snapshot.memory.object() ||
            current->flags() != snapshot.flags || !allows(current->flags(), access))
            return tay::Err(AddrSpaceError::MappingChanged(page, snapshot.generation));
        auto existing = space_->query(page);
        if (existing)
            return {};
        if (!existing.error().is<memory::PagingError::MissingMapping>())
            return tay::Err(page_table_error(page, existing.error()));
        auto mapped = space_->map(page, *physical, PAGE_SIZE, snapshot.flags);
        if (!mapped)
            return tay::Err(page_table_error(page, mapped.error()));
        return {};
    }

    void AddrSpace::activate() noexcept {
        // 当前活跃地址空间是 CpuLocal 状态。这里不能写 AddrSpace 生命周期字段，否则同一
        // Process 的两个 CPU 切换会对普通共享字段产生 data race。
        space_->activate();
    }

    bool AddrSpace::active_local() const noexcept {
        return memory::active_user_vm() == space_.get();
    }

    AddrSpaceState AddrSpace::state() const noexcept {
        auto state = state_.lock();
        return state->state;
    }
}  // namespace task
