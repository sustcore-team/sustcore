/**
 * @file address_space.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief AddressSpace VMA 管理、缺页映射和页表生命周期。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <obj/address_space.h>
#include <tay/lock.h>

#include <new>
#include <utility>

namespace task {
    namespace {
        struct FaultSnapshot final {
            cap::CapabilityRef<memory::MemorySegment> memory{};
            VirArea area{};
            size_t segment_offset = 0;
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

        [[nodiscard]] memory::PageFlags segment_allowed_flags(u64_t rights) noexcept {
            return memory::PageFlags{
                .readable   = (rights & cap::RIGHT_READ) != 0,
                .writable   = (rights & cap::RIGHT_WRITE) != 0,
                .executable = false,
                .user       = true,
                .global     = false,
            };
        }

        [[nodiscard]] AddressSpaceError page_table_error(
            VirAddr page, const memory::PagingError &error) noexcept {
            return AddressSpaceError::PageTableFailed(page, error.code());
        }
    }  // namespace

    tay::expected<cap::ObjectRef<AddressSpace>, AddressSpaceError> AddressSpace::create() noexcept {
        auto client = tay::create_unique<memory::ClientSpace, memory::PagingError>();
        if (!client)
            return tay::Err(client.error().code() == kernel::KernelError::PagingError::OUT_OF_MEMORY
                                ? AddressSpaceError::OutOfMemory()
                                : page_table_error({}, client.error()));
        auto *object = new (std::nothrow) AddressSpace(std::move(*client));
        if (object == nullptr)
            return tay::Err(AddressSpaceError::OutOfMemory());
        return cap::ObjectRef<AddressSpace>(*object);
    }

    AddressSpace::~AddressSpace() noexcept {
        state_ = AddressSpaceState::RETIRING;
        while (!vmas_.empty()) {
            auto *vma       = vmas_.pop_front();
            const auto area = vma->area();
            for (VirAddr address = area.begin; address < area.end; address += PAGE_SIZE) {
                auto mapping = space_->query(address);
                if (mapping)
                    (void)space_->unmap(address, PAGE_SIZE);
            }
            delete vma;
        }
    }

    tay::expected<VMA *, AddressSpaceError> AddressSpace::add_vma(
        const cap::CapabilityRef<memory::MemorySegment> &segment, VirArea area,
        size_t segment_offset, memory::PageFlags flags) noexcept {
        if (!segment || segment.object() == nullptr)
            return tay::Err(AddressSpaceError::InvalidSegment());
        if (area.nullable() || !area.begin.aligned<PAGE_SIZE>() || !area.end.aligned<PAGE_SIZE>() ||
            !is_user_vaddr(area.begin) || !is_user_vaddr(area.end - 1))
            return tay::Err(AddressSpaceError::InvalidArea(area));
        if (!valid_flags(flags))
            return tay::Err(AddressSpaceError::InvalidFlags(flags));
        const auto allowed = segment_allowed_flags(segment.rights());
        if ((flags.writable && !allowed.writable) || (flags.readable && !allowed.readable))
            return tay::Err(AddressSpaceError::AccessDenied(
                flags.writable ? memory::FaultAccess::WRITE : memory::FaultAccess::READ, allowed));
        const size_t segment_size = segment.object()->size();
        if (segment_offset > segment_size)
            return tay::Err(
                AddressSpaceError::SegmentOffsetOutOfRange(segment_offset, segment_size));
        if (area.size() > segment_size - segment_offset)
            return tay::Err(AddressSpaceError::MappingExceedsSegment(segment_offset, area.size(),
                                                                     segment_size));
        kernel::lock_guard<tay::spinlock> guard(lock_);
        for (auto iterator = vmas_.begin(); iterator != vmas_.end(); ++iterator) {
            if (tay::is_intersecting((*iterator)->area(), area))
                return tay::Err(AddressSpaceError::VmaOverlap(area, (*iterator)->area()));
            if ((*iterator)->area().begin >= area.begin) {
                auto *vma = new (std::nothrow) VMA(segment, area, segment_offset, flags);
                if (vma == nullptr)
                    return tay::Err(AddressSpaceError::OutOfMemory());
                vmas_.insert(iterator, vma);
                ++vma_generation_;
                state_ = AddressSpaceState::CONFIGURING;
                return vma;
            }
        }
        auto *vma = new (std::nothrow) VMA(segment, area, segment_offset, flags);
        if (vma == nullptr)
            return tay::Err(AddressSpaceError::OutOfMemory());
        vmas_.push_back(vma);
        ++vma_generation_;
        state_ = AddressSpaceState::CONFIGURING;
        return vma;
    }

    tay::expected<void, AddressSpaceError> AddressSpace::remove_vma(VMA &vma) noexcept {
        kernel::lock_guard<tay::spinlock> guard(lock_);
        bool owned = false;
        for (auto iterator = vmas_.begin(); iterator != vmas_.end(); ++iterator) {
            if (*iterator != &vma)
                continue;
            owned = true;
            break;
        }
        if (!owned)
            return tay::Err(AddressSpaceError::VmaNotOwned());
        const auto area = vma.area();
        for (VirAddr address = area.begin; address < area.end; address += PAGE_SIZE) {
            auto mapping = space_->query(address);
            if (mapping) {
                auto unmapped = space_->unmap(address, PAGE_SIZE);
                if (!unmapped)
                    return tay::Err(page_table_error(address, unmapped.error()));
            }
        }
        (void)vmas_.remove(&vma);
        ++vma_generation_;
        delete &vma;
        return {};
    }

    VMA *AddressSpace::locate_locked(VirAddr address) noexcept {
        for (auto iterator = vmas_.begin(); iterator != vmas_.end(); ++iterator)
            if (tay::within((*iterator)->area(), address))
                return *iterator;
        return nullptr;
    }

    const VMA *AddressSpace::locate_locked(VirAddr address) const noexcept {
        for (auto iterator = vmas_.begin(); iterator != vmas_.end(); ++iterator)
            if (tay::within((*iterator)->area(), address))
                return *iterator;
        return nullptr;
    }

    tay::expected<memory::PageMapping, AddressSpaceError> AddressSpace::query(
        VirAddr address) const noexcept {
        auto result = space_->query(address);
        if (!result)
            return tay::Err(page_table_error(address, result.error()));
        return *result;
    }

    tay::expected<void, AddressSpaceError> AddressSpace::handle_page_fault(
        VirAddr address, memory::FaultAccess access) noexcept {
        const VirAddr page = address.page_align_down();
        FaultSnapshot snapshot{};
        {
            kernel::lock_guard<tay::spinlock> guard(lock_);
            auto *vma = locate_locked(address);
            if (vma == nullptr)
                return tay::Err(AddressSpaceError::UnmappedAddress(address));
            if (!allows(vma->flags(), access))
                return tay::Err(AddressSpaceError::AccessDenied(access, vma->flags()));
            auto existing = space_->query(page);
            if (existing)
                return {};
            if (!existing.error().is<memory::PagingError::MissingMapping>())
                return tay::Err(page_table_error(page, existing.error()));
            snapshot = FaultSnapshot{
                .memory         = vma->memory(),
                .area           = vma->area(),
                .segment_offset = vma->segment_offset(),
                .flags          = vma->flags(),
                .generation     = vma_generation_,
            };
        }

        const size_t offset = snapshot.segment_offset + (page - snapshot.area.begin);
        auto physical       = snapshot.memory.object()->ensure_page(offset);
        if (!physical)
            return tay::Err(
                AddressSpaceError::BackingAllocationFailed(offset, physical.error().code()));

        kernel::lock_guard<tay::spinlock> guard(lock_);
        auto *current = locate_locked(address);
        if (vma_generation_ != snapshot.generation || current == nullptr ||
            current->area() != snapshot.area ||
            current->segment_offset() != snapshot.segment_offset ||
            current->memory().object() != snapshot.memory.object() ||
            current->flags() != snapshot.flags || !allows(current->flags(), access))
            return tay::Err(AddressSpaceError::MappingChanged(page, snapshot.generation));
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

    void AddressSpace::activate() noexcept {
        space_->activate();
        state_ = AddressSpaceState::ACTIVE;
    }
}  // namespace task
