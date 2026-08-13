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
    }  // namespace

    tay::expected<cap::ObjectRef<AddressSpace>, tay::error_code> AddressSpace::create() noexcept {
        auto client = tay::create_unique<memory::ClientSpace, tay::error_code>();
        if (!client)
            return tay::Err(client.error());
        auto *object = new (std::nothrow) AddressSpace(std::move(*client));
        if (object == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
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

    tay::expected<VMA *, tay::error_code> AddressSpace::add_vma(
        const cap::CapabilityRef<memory::MemorySegment> &segment, VirArea area,
        size_t segment_offset, memory::PageFlags flags) noexcept {
        if (!segment || segment.object() == nullptr || area.nullable() ||
            !area.begin.aligned<PAGE_SIZE>() || !area.end.aligned<PAGE_SIZE>() ||
            !is_user_vaddr(area.begin) || !is_user_vaddr(area.end - 1) || !valid_flags(flags) ||
            (flags.writable && !segment.allows(cap::RIGHT_WRITE)) ||
            (flags.readable && !segment.allows(cap::RIGHT_READ)) ||
            segment_offset > segment.object()->size() ||
            area.size() > segment.object()->size() - segment_offset)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        kernel::lock_guard<tay::spinlock> guard(lock_);
        for (auto iterator = vmas_.begin(); iterator != vmas_.end(); ++iterator) {
            if (tay::is_intersecting((*iterator)->area(), area))
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            if ((*iterator)->area().begin >= area.begin) {
                auto *vma = new (std::nothrow) VMA(segment, area, segment_offset, flags);
                if (vma == nullptr)
                    return tay::Err(tay::error_code::OUT_OF_MEMORY);
                vmas_.insert(iterator, vma);
                ++vma_generation_;
                state_ = AddressSpaceState::CONFIGURING;
                return vma;
            }
        }
        auto *vma = new (std::nothrow) VMA(segment, area, segment_offset, flags);
        if (vma == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        vmas_.push_back(vma);
        ++vma_generation_;
        state_ = AddressSpaceState::CONFIGURING;
        return vma;
    }

    tay::expected<void, tay::error_code> AddressSpace::remove_vma(VMA &vma) noexcept {
        kernel::lock_guard<tay::spinlock> guard(lock_);
        if (!vma.linked())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        const auto area = vma.area();
        for (VirAddr address = area.begin; address < area.end; address += PAGE_SIZE) {
            auto mapping = space_->query(address);
            if (mapping) {
                auto unmapped = space_->unmap(address, PAGE_SIZE);
                if (!unmapped)
                    return tay::Err(unmapped.error());
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

    tay::expected<void, tay::error_code> AddressSpace::handle_page_fault(
        VirAddr address, memory::FaultAccess access) noexcept {
        const VirAddr page = address.page_align_down();
        FaultSnapshot snapshot{};
        {
            kernel::lock_guard<tay::spinlock> guard(lock_);
            auto *vma = locate_locked(address);
            if (vma == nullptr || !allows(vma->flags(), access))
                return tay::Err(tay::error_code::OUT_OF_RANGE);
            if (space_->query(page))
                return {};
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
            return tay::Err(physical.error());

        kernel::lock_guard<tay::spinlock> guard(lock_);
        auto *current = locate_locked(address);
        if (vma_generation_ != snapshot.generation || current == nullptr ||
            current->area() != snapshot.area ||
            current->segment_offset() != snapshot.segment_offset ||
            current->memory().object() != snapshot.memory.object() ||
            current->flags() != snapshot.flags || !allows(current->flags(), access))
            return tay::Err(tay::error_code::OUT_OF_RANGE);
        if (space_->query(page))
            return {};
        return space_->map(page, *physical, PAGE_SIZE, snapshot.flags);
    }

    void AddressSpace::activate() noexcept {
        space_->activate();
        state_ = AddressSpaceState::ACTIVE;
    }
}  // namespace task
