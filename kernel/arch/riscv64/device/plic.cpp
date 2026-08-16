/**
 * @file plic.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V PLIC claim/complete 与 IRQ domain 实现
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/csr.h>
#include <arch/riscv64/device/plic.h>
#include <device/catalog.h>
#include <log.h>

#include <new>

namespace riscv64::device::interrupt {
    namespace {
        constinit tay::unique_ptr<Plic> plic_instance;

        [[nodiscard]] bool compatible(const char *value) noexcept {
            if (value == nullptr)
                return false;
            constexpr const char *expected[] = {"riscv,plic0", "sifive,plic-1.0.0"};
            for (const auto *candidate : expected) {
                size_t index = 0;
                while (candidate[index] != '\0' && value[index] == candidate[index]) ++index;
                if (candidate[index] == '\0' && value[index] == '\0')
                    return true;
            }
            return false;
        }
    }  // namespace

    volatile u32_t *Plic::reg(size_t offset) const noexcept {
        return reinterpret_cast<volatile u32_t *>(mmio_->kernel_base().arith() + offset);
    }

    volatile u32_t *Plic::enable_reg(u32_t hardware_irq) const noexcept {
        const size_t word = hardware_irq / ENABLE_WORD_BITS;
        return reg(ENABLE_BASE + context_id_ * ENABLE_STRIDE + word * sizeof(u32_t));
    }

    tay::expected<void, PlicError> Plic::validate(u32_t hardware_irq) const noexcept {
        if (!mmio_ || !mmio_->mapped())
            return tay::Err(PlicError::MissingMmio());
        if (hardware_irq == 0 || hardware_irq > MAX_SOURCES)
            return tay::Err(PlicError::SourceOutOfRange(hardware_irq, MAX_SOURCES));
        return {};
    }

    tay::expected<void, PlicError> Plic::validate(const IrqClaim &claim) const noexcept {
        if (claim.domain != this || claim.generation == 0)
            return tay::Err(PlicError::InvalidClaim());
        return validate(claim.hardware_irq);
    }

    tay::expected<void, PlicError> Plic::initialize() noexcept {
        if (!mmio_)
            return tay::Err(PlicError::MissingMmio());
        auto mapped = mmio_->map_to_kernel();
        if (!mapped)
            return tay::Err(PlicError::MmioFailed(mapped.error().code()));

        const auto area_size = mmio_->area().size();
        const size_t context_count =
            area_size <= CONTEXT_BASE ? 0 : (area_size - CONTEXT_BASE) / CONTEXT_STRIDE;
        if (context_count == 0)
            return tay::Err(PlicError::MissingContext());
        if (context_id_ >= context_count)
            return tay::Err(PlicError::ContextOutOfRange(
                context_id_,
                static_cast<u32_t>(context_count > UINT32_MAX ? UINT32_MAX : context_count)));

        *reg(CONTEXT_BASE + context_id_ * CONTEXT_STRIDE) = 0;
        for (u32_t irq = 1; irq <= MAX_SOURCES; ++irq) {
            TAY_TRYV(set_priority_detailed(irq, 1));
            TAY_TRYV(mask_detailed(irq));
        }
        (void)hal::csr::set_bits<hal::csr::CSR::SIE>(xlen_t{1} << 9);
        return {};
    }

    tay::expected<u32_t, tay::error_code> Plic::claim() noexcept {
        auto claimed = claim_detailed();
        if (!claimed)
            return tay::Err(to_tay_error(claimed.error()));
        return *claimed;
    }

    tay::expected<u32_t, PlicError> Plic::claim_detailed() noexcept {
        if (!mmio_ || !mmio_->mapped())
            return tay::Err(PlicError::MissingMmio());
        auto locked         = register_lock_.lock();
        const u32_t claimed = *reg(CONTEXT_BASE + context_id_ * CONTEXT_STRIDE + CLAIM_OFFSET);
        if (claimed > MAX_SOURCES)
            return tay::Err(PlicError::SourceOutOfRange(claimed, MAX_SOURCES));
        return claimed;
    }

    tay::expected<void, tay::error_code> Plic::ack(const IrqClaim &claim) noexcept {
        // PLIC 的 claim 读操作已经完成硬件确认；这里仅验证 token。
        return validate(claim).transform_error(
            [](const PlicError &error) noexcept { return to_tay_error(error); });
    }

    tay::expected<void, tay::error_code> Plic::eoi(const IrqClaim &claim) noexcept {
        // PLIC 没有独立的 EOI 寄存器，保持接口语义上的独立阶段。
        return validate(claim).transform_error(
            [](const PlicError &error) noexcept { return to_tay_error(error); });
    }

    tay::expected<void, tay::error_code> Plic::complete(const IrqClaim &claim) noexcept {
        return complete_detailed(claim).transform_error(
            [](const PlicError &error) noexcept { return to_tay_error(error); });
    }

    tay::expected<void, PlicError> Plic::complete_detailed(const IrqClaim &claim) noexcept {
        TAY_TRYV(validate(claim));
        auto locked                                                      = register_lock_.lock();
        *reg(CONTEXT_BASE + context_id_ * CONTEXT_STRIDE + CLAIM_OFFSET) = claim.hardware_irq;
        return {};
    }

    tay::expected<void, tay::error_code> Plic::mask(u32_t hardware_irq) noexcept {
        return mask_detailed(hardware_irq).transform_error([](const PlicError &error) noexcept {
            return to_tay_error(error);
        });
    }

    tay::expected<void, PlicError> Plic::mask_detailed(u32_t hardware_irq) noexcept {
        TAY_TRYV(validate(hardware_irq));
        auto locked  = register_lock_.lock();
        auto *entry  = enable_reg(hardware_irq);
        *entry      &= ~enable_mask(hardware_irq);
        return {};
    }

    tay::expected<void, tay::error_code> Plic::unmask(u32_t hardware_irq) noexcept {
        return unmask_detailed(hardware_irq).transform_error([](const PlicError &error) noexcept {
            return to_tay_error(error);
        });
    }

    tay::expected<void, PlicError> Plic::unmask_detailed(u32_t hardware_irq) noexcept {
        TAY_TRYV(validate(hardware_irq));
        auto locked  = register_lock_.lock();
        auto *entry  = enable_reg(hardware_irq);
        *entry      |= enable_mask(hardware_irq);
        return {};
    }

    tay::expected<void, tay::error_code> Plic::set_priority(u32_t hardware_irq,
                                                            u32_t priority) noexcept {
        return set_priority_detailed(hardware_irq, priority)
            .transform_error([](const PlicError &error) noexcept { return to_tay_error(error); });
    }

    tay::expected<void, PlicError> Plic::set_priority_detailed(u32_t hardware_irq,
                                                               u32_t priority) noexcept {
        TAY_TRYV(validate(hardware_irq));
        if (priority > MAX_PRIORITY)
            return tay::Err(PlicError::InvalidPriority(priority, MAX_PRIORITY));
        auto locked                                        = register_lock_.lock();
        *reg(PRIORITY_BASE + hardware_irq * sizeof(u32_t)) = priority;
        return {};
    }

    tay::expected<void, tay::error_code> Plic::initialize_from_catalog() noexcept {
        if (plic_instance)
            return {};

        const auto &devices = catalog();
        for (auto it = devices.controllers_begin(); it != devices.controllers_end(); ++it) {
            if (!compatible(it->compatible) || !it->first_mmio.present)
                continue;

            auto mmio = MmioObject::create(it->first_mmio.area);
            if (!mmio) {
                PlicError error = PlicError::MmioFailed(mmio.error().code());
                kernel::log::error("PLIC 初始化失败: {}", error);
                return tay::Err(to_tay_error(error));
            }
            u32_t context_id = 0;
            if (const auto *bsp = devices.bsp(); bsp != nullptr) {
                const u64_t candidate = bsp->hardware_id * 2 + 1;
                if (candidate <= UINT32_MAX)
                    context_id = static_cast<u32_t>(candidate);
            }
            auto *driver = new (std::nothrow) Plic(it->id, context_id, std::move(*mmio));
            if (driver == nullptr) {
                PlicError error =
                    PlicError::PlicAllocationFailed(kernel::KernelError::TayError::OUT_OF_MEMORY);
                kernel::log::error("PLIC 初始化失败: {}", error);
                return tay::Err(to_tay_error(error));
            }
            tay::unique_ptr<Plic> owner(driver);

            auto initialized = owner->initialize();
            if (!initialized) {
                kernel::log::error("PLIC 初始化失败: {}", initialized.error());
                return tay::Err(to_tay_error(initialized.error()));
            }
            auto registered = register_domain(*owner);
            if (!registered) {
                const auto cause = kernel::from_tay_error(registered.error());
                PlicError error  = PlicError::DomainRegistrationFailed(
                    cause ? *cause : kernel::KernelError::TayError::INTERNAL);
                kernel::log::error("PLIC 初始化失败: {}", error);
                return tay::Err(to_tay_error(error));
            }
            plic_instance = std::move(owner);
            kernel::log::info("PLIC IRQ domain 已初始化: controller={:#x}, context={}, lines={}",
                              it->id.local_id, context_id, Plic::MAX_SOURCES);
            return {};
        }
        PlicError error = PlicError::ControllerNotFound();
        kernel::log::error("PLIC 初始化失败: {}", error);
        return tay::Err(to_tay_error(error));
    }
}  // namespace riscv64::device::interrupt
