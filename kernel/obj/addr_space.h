/**
 * @file addr_space.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability AddrSpace 对象、VMA 链表和按需用户页映射。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <memory/virtual/user/vm.h>
#include <obj/kobject.h>
#include <obj/mem_seg.h>
#include <synchronized.h>
#include <error/addr_space.h>
#include <task/vma.h>
#include <tay/expected.h>
#include <tay/list.h>
#include <tay/unique_ptr.h>

#include <cstddef>

namespace task {
    enum class AddrSpaceState : u8_t {
        EMPTY,
        CONFIGURING,
        RETIRING,
    };

    class AddrSpace final : public cap::TypedKObject<AddrSpace, cap::ObjectType::ADDRESS_SPACE> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::ADDRESS_SPACE;

        [[nodiscard]] static tay::expected<cap::KObjectRef<AddrSpace>, AddrSpaceError>
        create() noexcept;

        AddrSpace(const AddrSpace &)            = delete;
        AddrSpace &operator=(const AddrSpace &) = delete;
        AddrSpace(AddrSpace &&)                 = delete;
        AddrSpace &operator=(AddrSpace &&)      = delete;
        ~AddrSpace() noexcept;

        [[nodiscard]] tay::expected<VMA *, AddrSpaceError> add_vma(
            const cap::CRef<memory::MemSeg> &segment, VirArea area, size_t seg_offset,
            memory::PageFlags flags) noexcept;
        [[nodiscard]] tay::expected<void, AddrSpaceError> remove_vma(VMA &vma) noexcept;
        [[nodiscard]] tay::expected<void, AddrSpaceError> handle_fault(
            VirAddr address, memory::FaultAccess access) noexcept;

        [[nodiscard]] tay::expected<memory::PageMapping, AddrSpaceError> query(
            VirAddr address) const noexcept;

        void activate() noexcept;
        [[nodiscard]] bool active_local() const noexcept;

        /** @brief 返回 VMA 配置状态的锁内快照；激活 CPU 不改变此共享状态。 */
        [[nodiscard]] AddrSpaceState state() const noexcept;

    private:
        explicit AddrSpace(tay::unique_ptr<memory::UserVm> &&space) noexcept
            : space_(std::move(space)) {}

        using vma_list = tay::intrusive_list<VMA, VMAHookLocator>;

        struct State final {
            vma_list vmas{};
            u64_t generation     = 0;
            AddrSpaceState state = AddrSpaceState::EMPTY;
        };

        struct VmaInsert final {
            vma_list::iterator before;
            VMA *overlap = nullptr;
        };

        [[nodiscard]] static VMA *find_vma_locked(State &state, VirAddr address) noexcept;
        [[nodiscard]] static const VMA *find_vma_locked(const State &state,
                                                        VirAddr address) noexcept;
        [[nodiscard]] static VmaInsert find_insert_locked(State &state, VirArea area) noexcept;
        [[nodiscard]] tay::expected<void, AddrSpaceError> unmap_vma_locked(VirArea area) noexcept;

        tay::unique_ptr<memory::UserVm> space_{};
        kernel::simple_synchronized<State> state_{};
    };
}  // namespace task
