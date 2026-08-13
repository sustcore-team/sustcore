/**
 * @file address_space.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability AddressSpace 对象、VMA 链表和按需用户页映射。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <memory/virtual/client/client_space.h>
#include <obj/kernel_object.h>
#include <obj/memory_segment.h>
#include <task/vma.h>
#include <tay/expected.h>
#include <tay/list.h>
#include <tay/spinlock.h>
#include <tay/unique_ptr.h>

#include <cstddef>

namespace task {
    enum class AddressSpaceState : u8_t {
        EMPTY,
        CONFIGURING,
        ACTIVE,
        RETIRING,
    };

    class AddressSpace final
        : public cap::TypedKernelObject<AddressSpace, cap::ObjectType::ADDRESS_SPACE> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::ADDRESS_SPACE;

        [[nodiscard]] static tay::expected<cap::ObjectRef<AddressSpace>, tay::error_code>
        create() noexcept;

        AddressSpace(const AddressSpace &)            = delete;
        AddressSpace &operator=(const AddressSpace &) = delete;
        AddressSpace(AddressSpace &&)                 = delete;
        AddressSpace &operator=(AddressSpace &&)      = delete;
        ~AddressSpace() noexcept;

        [[nodiscard]] tay::expected<VMA *, tay::error_code> add_vma(
            const cap::CapabilityRef<memory::MemorySegment> &segment, VirArea area,
            size_t segment_offset, memory::PageFlags flags) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> remove_vma(VMA &vma) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> handle_page_fault(
            VirAddr address, memory::FaultAccess access) noexcept;

        [[nodiscard]] tay::expected<memory::PageMapping, tay::error_code> query(
            VirAddr address) const noexcept {
            return space_->query(address);
        }

        void activate() noexcept;

        [[nodiscard]] AddressSpaceState state() const noexcept {
            return state_;
        }

    private:
        explicit AddressSpace(tay::unique_ptr<memory::ClientSpace> &&space) noexcept
            : space_(std::move(space)) {}

        using vma_list = tay::intrusive_list<VMA, VMAHookLocator>;

        [[nodiscard]] VMA *locate_locked(VirAddr address) noexcept;
        [[nodiscard]] const VMA *locate_locked(VirAddr address) const noexcept;

        tay::unique_ptr<memory::ClientSpace> space_{};
        mutable tay::spinlock lock_{};
        vma_list vmas_{};
        u64_t vma_generation_    = 0;
        AddressSpaceState state_ = AddressSpaceState::EMPTY;
    };
}  // namespace task
