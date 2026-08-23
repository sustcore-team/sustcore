/**
 * @file vm.h
 * @brief 全局内核页表的唯一所有者。
 */

#pragma once

#include <boot/boot.h>
#include <memory/virtual/pt.h>
#include <memory/virtual/pt_root.h>
#include <sustcore/addr.h>
#include <synchronized.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <atomic>

namespace memory {
    inline constexpr PtOwnerId KERNEL_PAGE_TABLE_OWNER = 1;

    class KernelMM;

    struct RootSlot final {
        size_t index               = 0;
        PageTable::EntryType entry = 0;
        bool published             = false;
    };

    class KernelVm final {
    public:
        [[nodiscard]] static tay::expected<void, PagingError> initialize(
            const BootInfoHeader &bootinfo) noexcept;

        KernelVm(const KernelVm &)            = delete;
        KernelVm &operator=(const KernelVm &) = delete;
        KernelVm(KernelVm &&)                 = delete;
        KernelVm &operator=(KernelVm &&)      = delete;

        [[nodiscard]] tay::expected<PageMapping, PagingError> query(HvaAddr address) const noexcept;
        /** @brief 在内核高半区建立物理设备寄存器映射。 */
        [[nodiscard]] tay::expected<KvaAddr, PagingError> map_device(PhyAddr physical, size_t bytes,
                                                                     PageFlags flags) noexcept;
        /** @brief 移除由 map_device() 建立的设备寄存器映射。 */
        [[nodiscard]] tay::expected<void, PagingError> unmap_device(PhyAddr physical,
                                                                    size_t bytes) noexcept;
        [[nodiscard]] PhyAddr root() const noexcept;
        [[nodiscard]] PhyAddr guard_root() const noexcept {
            return guard_root_;
        }
        [[nodiscard]] RootBinding binding() const noexcept;
        void activate() noexcept;

        [[nodiscard]] tay::expected<RootSlot, PagingError> published_slot(
            HvaAddr address) const noexcept;
        [[nodiscard]] tay::expected<void, PagingError> copy_high_slots_to(
            PageTable &client) const noexcept;

    private:
        friend class KernelMM;
        friend bool kernel_vm_ready() noexcept;
        friend KernelVm *try_kernel_vm() noexcept;

        struct PtState final {
            constexpr PtState() noexcept = default;
            explicit PtState(PhyAddr root) noexcept
                : table(root, PtKind::KERNEL, KERNEL_PAGE_TABLE_OWNER) {}
            PageTable table;
        };

        constexpr KernelVm() noexcept = default;

        [[nodiscard]] tay::expected<void, PagingError> map_high(HvaAddr address, PhyAddr physical,
                                                                size_t bytes,
                                                                PageFlags flags) noexcept;
        [[nodiscard]] tay::expected<void, PagingError> unmap_high(HvaAddr address,
                                                                  size_t bytes) noexcept;
        [[nodiscard]] tay::expected<void, PagingError> protect_high(HvaAddr address, size_t bytes,
                                                                    PageFlags flags) noexcept;
        /** @brief 映射已验证 AP trampoline 的唯一低地址执行别名。 */
        [[nodiscard]] tay::expected<void, PagingError> map_trampoline(
            PhyAddr physical) noexcept;

        kernel::synchronized<PtState> page_table_;
        PhyAddr guard_root_{};

        static KernelVm instance_;
        static std::atomic<bool> ready_;
    };

    [[nodiscard]] bool kernel_vm_ready() noexcept;
    [[nodiscard]] KernelVm *try_kernel_vm() noexcept;
    KernelVm &kernel_vm() noexcept;
    void init_kernel_vm(const BootInfoHeader &bootinfo) noexcept;
}  // namespace memory
