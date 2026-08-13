/**
 * @file client_space.h
 * @brief 私有低半区页表及架构根绑定。
 */

#pragma once

#include <memory/virtual/kernel/kernel_space.h>
#include <memory/virtual/page_table.h>
#include <memory/virtual/root_binding.h>
#include <sustcore/addr.h>
#include <synchronized.h>
#include <tay/expected.h>

namespace memory {
    enum class BorrowedSlotRepair : u8_t {
        REPAIRED,
        LOCAL_SLOT_PRESENT,
        GLOBAL_SLOT_ABSENT,
    };

    class ClientSpace final {
    public:
        [[nodiscard]] static tay::expected<ClientSpace *, tay::error_code> create() noexcept;

        ClientSpace(const ClientSpace &)            = delete;
        ClientSpace &operator=(const ClientSpace &) = delete;
        ClientSpace(ClientSpace &&)                 = delete;
        ClientSpace &operator=(ClientSpace &&)      = delete;

        [[nodiscard]] tay::expected<void, tay::error_code> map(VirAddr address, PhyAddr physical,
                                                               size_t bytes,
                                                               PageFlags flags) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> unmap(VirAddr address,
                                                                 size_t bytes) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> protect(VirAddr address, size_t bytes,
                                                                   PageFlags flags) noexcept;
        [[nodiscard]] tay::expected<PageMapping, tay::error_code> query(
            VirAddr address) const noexcept;

        [[nodiscard]] tay::expected<BorrowedSlotRepair, tay::error_code>
        repair_missing_borrowed_kernel_slot(HvaAddr address) noexcept;

        [[nodiscard]] RootBinding binding() const noexcept;
        void activate() noexcept;

    private:
        struct Resources final {
            PhyAddr root{};
            PageTableOwnerId owner = 0;
            u16_t asid             = 0;
        };

        explicit ClientSpace(Resources resources) noexcept
            : page_table_(resources.root, PageTableKind::USER, resources.owner),
              owner_(resources.owner),
              asid_(resources.asid) {}

        [[nodiscard]] tay::expected<void, tay::error_code> init() noexcept;

        kernel::synchronized<PageTable> page_table_;
        PageTableOwnerId owner_ = 0;
        u16_t asid_             = 0;
    };

    [[nodiscard]] ClientSpace *active_client_space() noexcept;
    void activate_kernel_space() noexcept;
}  // namespace memory
