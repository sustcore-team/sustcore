/**
 * @file vm.h
 * @brief 私有低半区页表及架构根绑定。
 */

#pragma once

#include <memory/virtual/kernel/vm.h>
#include <memory/virtual/pt.h>
#include <memory/virtual/pt_root.h>
#include <sustcore/addr.h>
#include <synchronized.h>
#include <tay/expected.h>

namespace memory {
    enum class BorrowedSlotFix : u8_t {
        REPAIRED,
        LOCAL_SLOT_PRESENT,
        GLOBAL_SLOT_ABSENT,
    };

    class UserVm final {
    public:
        [[nodiscard]] static tay::expected<UserVm *, PagingError> create() noexcept;

        UserVm(const UserVm &)            = delete;
        UserVm &operator=(const UserVm &) = delete;
        UserVm(UserVm &&)                 = delete;
        UserVm &operator=(UserVm &&)      = delete;

        [[nodiscard]] tay::expected<void, PagingError> map(VirAddr address, PhyAddr physical,
                                                           size_t bytes, PageFlags flags) noexcept;
        [[nodiscard]] tay::expected<void, PagingError> unmap(VirAddr address,
                                                             size_t bytes) noexcept;
        [[nodiscard]] tay::expected<void, PagingError> protect(VirAddr address, size_t bytes,
                                                               PageFlags flags) noexcept;
        [[nodiscard]] tay::expected<PageMapping, PagingError> query(VirAddr address) const noexcept;

        [[nodiscard]] tay::expected<BorrowedSlotFix, PagingError> fix_borrowed_slot(
            HvaAddr address) noexcept;

        [[nodiscard]] RootBinding binding() const noexcept;
        void activate() noexcept;

    private:
        struct InitArgs final {
            PhyAddr root{};
            PtOwnerId owner = 0;
            u16_t asid      = 0;
        };

        explicit UserVm(InitArgs init_args) noexcept
            : page_table_(init_args.root, PtKind::USER, init_args.owner, init_args.asid),
              owner_(init_args.owner),
              asid_(init_args.asid) {}

        [[nodiscard]] tay::expected<void, PagingError> init() noexcept;

        kernel::synchronized<PageTable> page_table_;
        PtOwnerId owner_ = 0;
        u16_t asid_      = 0;
    };

    [[nodiscard]] UserVm *active_user_vm() noexcept;
    void activate_kernel_vm() noexcept;
}  // namespace memory
