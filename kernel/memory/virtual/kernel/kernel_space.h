/**
 * @file kernel_space.h
 * @brief 全局内核页表的唯一所有者。
 */

#pragma once

#include <boot/boot.h>
#include <memory/virtual/page_table.h>
#include <memory/virtual/root_binding.h>
#include <sustcore/addr.h>
#include <synchronized.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <atomic>

namespace memory {
    inline constexpr PageTableOwnerId KERNEL_PAGE_TABLE_OWNER = 1;

    class KernelMM;

    struct RootSlotSnapshot final {
        size_t index               = 0;
        PageTable::EntryType entry = 0;
        bool published             = false;
    };

    class KernelSpace final {
    public:
        [[nodiscard]] static tay::expected<void, tay::error_code> initialize(
            const BootInfoHeader &bootinfo) noexcept;

        KernelSpace(const KernelSpace &)            = delete;
        KernelSpace &operator=(const KernelSpace &) = delete;
        KernelSpace(KernelSpace &&)                 = delete;
        KernelSpace &operator=(KernelSpace &&)      = delete;

        [[nodiscard]] tay::expected<PageMapping, tay::error_code> query(
            HvaAddr address) const noexcept;
        [[nodiscard]] PhyAddr root() const noexcept;
        [[nodiscard]] PhyAddr guard_root() const noexcept {
            return guard_root_;
        }
        [[nodiscard]] RootBinding binding() const noexcept;
        void activate() noexcept;

        [[nodiscard]] tay::expected<RootSlotSnapshot, tay::error_code> published_slot(
            HvaAddr address) const noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> copy_published_high_slots_to(
            PageTable &client) const noexcept;

    private:
        friend class KernelMM;
        friend bool kernel_space_ready() noexcept;
        friend KernelSpace *try_kernel_space() noexcept;

        struct PageTableState final {
            constexpr PageTableState() noexcept = default;
            explicit PageTableState(PhyAddr root) noexcept
                : table(root, PageTableKind::KERNEL, KERNEL_PAGE_TABLE_OWNER) {}
            PageTable table;
        };

        constexpr KernelSpace() noexcept = default;

        [[nodiscard]] tay::expected<void, tay::error_code> map_high(HvaAddr address,
                                                                    PhyAddr physical, size_t bytes,
                                                                    PageFlags flags) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> unmap_high(HvaAddr address,
                                                                      size_t bytes) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> protect_high(HvaAddr address,
                                                                        size_t bytes,
                                                                        PageFlags flags) noexcept;

        kernel::synchronized<PageTableState> page_table_;
        PhyAddr guard_root_{};

        static KernelSpace instance_;
        static std::atomic<bool> ready_;
    };

    [[nodiscard]] bool kernel_space_ready() noexcept;
    [[nodiscard]] KernelSpace *try_kernel_space() noexcept;
    KernelSpace &kernel_space() noexcept;
    void init_kernel_space(const BootInfoHeader &bootinfo) noexcept;
}  // namespace memory
