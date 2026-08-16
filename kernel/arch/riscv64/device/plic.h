/**
 * @file plic.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V PLIC IRQ domain 驱动
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/riscv64/device/plic_error.h>
#include <arch/riscv64/namespace.h>
#include <device/interrupt.h>
#include <device/mmio.h>
#include <synchronized.h>
#include <tay/unique_ptr.h>

namespace riscv64::device::interrupt {
    class Plic final : public IrqDomain {
    public:
        static constexpr u32_t MAX_SOURCES = 1023;

        [[nodiscard]] static tay::expected<void, tay::error_code>
        initialize_from_catalog() noexcept;

        Plic(const Plic &)            = delete;
        Plic &operator=(const Plic &) = delete;
        Plic(Plic &&)                 = delete;
        Plic &operator=(Plic &&)      = delete;
        ~Plic() noexcept              = default;

        [[nodiscard]] FirmwareId controller() const noexcept override {
            return controller_;
        }

        [[nodiscard]] u32_t line_count() const noexcept override {
            return MAX_SOURCES;
        }

        [[nodiscard]] tay::expected<u32_t, tay::error_code> claim() noexcept override;
        [[nodiscard]] tay::expected<void, tay::error_code> ack(
            const IrqClaim &claim) noexcept override;
        [[nodiscard]] tay::expected<void, tay::error_code> eoi(
            const IrqClaim &claim) noexcept override;
        [[nodiscard]] tay::expected<void, tay::error_code> complete(
            const IrqClaim &claim) noexcept override;
        [[nodiscard]] tay::expected<void, tay::error_code> mask(
            u32_t hardware_irq) noexcept override;
        [[nodiscard]] tay::expected<void, tay::error_code> unmask(
            u32_t hardware_irq) noexcept override;
        [[nodiscard]] tay::expected<void, tay::error_code> set_priority(
            u32_t hardware_irq, u32_t priority) noexcept override;

    private:
        static constexpr size_t PRIORITY_BASE   = 0x000000;
        static constexpr size_t ENABLE_BASE     = 0x002000;
        static constexpr size_t ENABLE_STRIDE   = 0x80;
        static constexpr size_t CONTEXT_BASE    = 0x200000;
        static constexpr size_t CONTEXT_STRIDE  = 0x1000;
        static constexpr size_t CLAIM_OFFSET    = 0x4;
        static constexpr u32_t ENABLE_WORD_BITS = 32;
        static constexpr u32_t MAX_PRIORITY     = 7;

        Plic(FirmwareId controller, u32_t context_id, cap::ObjectRef<MmioObject> mmio) noexcept
            : controller_(controller), context_id_(context_id), mmio_(std::move(mmio)) {}

        [[nodiscard]] tay::expected<void, PlicError> initialize() noexcept;
        [[nodiscard]] tay::expected<void, PlicError> validate(u32_t hardware_irq) const noexcept;
        [[nodiscard]] tay::expected<void, PlicError> validate(const IrqClaim &claim) const noexcept;
        [[nodiscard]] tay::expected<u32_t, PlicError> claim_detailed() noexcept;
        [[nodiscard]] tay::expected<void, PlicError> complete_detailed(
            const IrqClaim &claim) noexcept;
        [[nodiscard]] tay::expected<void, PlicError> mask_detailed(u32_t hardware_irq) noexcept;
        [[nodiscard]] tay::expected<void, PlicError> unmask_detailed(u32_t hardware_irq) noexcept;
        [[nodiscard]] tay::expected<void, PlicError> set_priority_detailed(u32_t hardware_irq,
                                                                           u32_t priority) noexcept;
        [[nodiscard]] volatile u32_t *reg(size_t offset) const noexcept;
        [[nodiscard]] volatile u32_t *enable_reg(u32_t hardware_irq) const noexcept;
        [[nodiscard]] u32_t enable_mask(u32_t hardware_irq) const noexcept {
            return u32_t{1} << (hardware_irq % ENABLE_WORD_BITS);
        }

        FirmwareId controller_{};
        u32_t context_id_ = 0;
        cap::ObjectRef<MmioObject> mmio_{};
        mutable kernel::synchronized<u8_t> register_lock_{};
    };
}  // namespace riscv64::device::interrupt
