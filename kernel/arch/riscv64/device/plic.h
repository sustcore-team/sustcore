/**
 * @file plic.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V PLIC 设备与驱动
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/device.h>
#include <device/model.h>
#include <device/resource.h>
#include <driver/int/base.h>

#include <unordered_map>
#include <vector>

namespace riscv {
    class Plic final : public driver::IrqChip {
    public:
        using ctx_t = size_t;

        struct Context {
            device::cpuid_t hart_id = 0;
            driver::virq_t external_virq = 0;
            ctx_t ctx_id = 0;
            bool enabled = false;
            bool completed = true;
        };

        ~Plic() override;

        [[nodiscard]]
        std::string_view compatible() const noexcept override;
        [[nodiscard]]
        driver::intc_t identifier() const noexcept;
        [[nodiscard]]
        driver::hwirq_t srccnt() const noexcept;
        [[nodiscard]]
        Result<void> enable_irq(driver::hwirq_t hw_irq) noexcept override;
        [[nodiscard]]
        Result<void> disable_irq(driver::hwirq_t hw_irq) noexcept override;
        [[nodiscard]]
        Result<void> set_priority(driver::hwirq_t hw_irq,
                                  driver::irq_prio_t prio) noexcept override;
        [[nodiscard]]
        Result<void> set_affinity(driver::hwirq_t hw_irq,
                                  driver::cpu_mask_t mask) noexcept override;
        [[nodiscard]]
        Result<void> ack(const driver::IrqEvent &event) noexcept override;
        [[nodiscard]]
        Result<void> set_trigger(driver::hwirq_t hw_irq,
                                 driver::IrqTrigger trigger) noexcept override;

    private:
        [[nodiscard]]
        Result<void> enable_irq_for(ctx_t ctx, driver::hwirq_t hw_irq) noexcept;
        [[nodiscard]]
        Result<void> disable_irq_for(ctx_t ctx, driver::hwirq_t hw_irq) noexcept;

        constexpr static size_t PLIC_MAX_CONTEXTS = 15872;
        constexpr static size_t PLIC_MAX_SOURCES = 1023;
        constexpr static const char *PLIC_COMPATIBLE_STRING = "riscv,plic0";

        driver::intc_t _identifier = device::INVALID_ICTRL_ID;
        size_t _srccnt = 0;
        size_t _ctxcnt = 0;
        ctx_t _first_enabled_ctx = PLIC_MAX_CONTEXTS;
        std::vector<Context> _contexts;
        std::unordered_map<driver::virq_t, ctx_t> _virq_to_context;
        driver::IrqDomain *_domain = nullptr;

        [[nodiscard]]
        Result<Context &> resolve_ctx(driver::virq_t parent_virq) const noexcept;
        [[nodiscard]]
        Result<device::VIrqResource *> resolve_virq_resource(
            driver::virq_t parent_virq) const noexcept;

        ctx_t _claimlist[PLIC_MAX_SOURCES + 1] = {};
        driver::hwirq_t claim(Context &ctx) noexcept;
        void handle_parent_irq(const driver::IrqEvent &event) noexcept;
        [[nodiscard]]
        Result<void> validate_source(driver::hwirq_t hw_irq) const noexcept;
        [[nodiscard]]
        Result<void> validate_context(size_t ctx_id) const noexcept;

        static driver::IrqManager &irqman() {
            return device::DeviceModel::inst().interrupt();
        }

        constexpr static size_t PLIC_SRCBITS_SIZE    = 128;
        constexpr static size_t PLIC_SRCBITS_ENTRIES = 32;

        static_assert(PLIC_SRCBITS_SIZE == (PLIC_MAX_SOURCES + 1) / 8,
                      "PLIC source bits size mismatch!");
        static_assert(PLIC_SRCBITS_ENTRIES == PLIC_SRCBITS_SIZE / 4,
                      "PLIC source bits entries mismatch!");

        using intprio_t = dword;
        constexpr static size_t PLIC_INTPRIO_SIZE = 0x1000;
        struct IntPrios {
            intprio_t int_prios[PLIC_MAX_SOURCES + 1];
        };
        static_assert(sizeof(IntPrios) == PLIC_INTPRIO_SIZE,
                      "PLIC IntPrios struct size mismatch!");
        static_assert(sizeof(intprio_t) == 4, "intprio_t size mismatch!");

        using ipb_t = dword;
        struct IPBs {
            ipb_t ipb[PLIC_SRCBITS_ENTRIES];
        };
        static_assert(sizeof(IPBs) == PLIC_SRCBITS_SIZE,
                      "PLIC IPBs struct size mismatch!");
        static_assert(sizeof(ipb_t) == 4, "ipb_t size mismatch!");

        using ieb_t = dword;
        constexpr static size_t PLIC_IEBS_SIZE = 0x1F0000;
        constexpr static size_t PLIC_IEB_SIZE = 0x80;
        struct IEBs {
            ieb_t ieb[PLIC_MAX_CONTEXTS][PLIC_SRCBITS_ENTRIES];
        };
        static_assert(sizeof(IEBs) == PLIC_IEBS_SIZE,
                      "PLIC IEBs struct size mismatch!");
        static_assert(sizeof(std::declval<IEBs>().ieb[0]) == PLIC_IEB_SIZE,
                      "PLIC IEB struct size mismatch!");
        static_assert(PLIC_IEBS_SIZE == PLIC_MAX_CONTEXTS * PLIC_SRCBITS_SIZE,
                      "PLIC IEBs size mismatch!");
        static_assert(sizeof(ieb_t) == 4, "ieb_t size mismatch!");

        constexpr static size_t PLIC_CRSR_SIZE = 0x1000;
        constexpr static size_t PLIC_THRESHOLD = 0x0;
        constexpr static size_t PLIC_CLAIM = 0x4;
        constexpr static size_t PLIC_COMPLETE = 0x4;
        constexpr static size_t PLIC_CRSRS_SIZE = 0x3e00000;
        struct CRSR {
            intprio_t threshold;
            union {
                dword claim;
                dword complete;
            };
            char padding[PLIC_CRSR_SIZE - 2 * sizeof(intprio_t)];
        };
        static_assert(sizeof(CRSR) == PLIC_CRSR_SIZE,
                      "PLIC crsr struct size mismatch!");
        static_assert(sizeof(intprio_t) == 4, "intprio_t size mismatch!");
        static_assert(offsetof(CRSR, threshold) == PLIC_THRESHOLD,
                      "threshold offset mismatch!");
        static_assert(offsetof(CRSR, claim) == PLIC_CLAIM,
                      "claim offset mismatch!");
        static_assert(offsetof(CRSR, complete) == PLIC_COMPLETE,
                      "complete offset mismatch!");
        struct CRSRs {
            CRSR crsrs[PLIC_MAX_CONTEXTS];
        };
        static_assert(sizeof(CRSRs) == PLIC_CRSRS_SIZE,
                      "PLIC CRSRs struct size mismatch!");
        static_assert(PLIC_CRSRS_SIZE == PLIC_MAX_CONTEXTS * PLIC_CRSR_SIZE,
                      "PLIC CRSRs size mismatch!");

        volatile char *_base = nullptr;
        constexpr static size_t PLIC_INTPRIO = 0x0;
        volatile IntPrios *_intprios_base = nullptr;
        constexpr static size_t PLIC_IPB = 0x1000;
        volatile IPBs *_ipbs_base = nullptr;
        constexpr static size_t PLIC_IEB = 0x2000;
        volatile IEBs *_iebs_base = nullptr;
        constexpr static size_t PLIC_CRSR = 0x200000;
        volatile CRSRs *_crsrs_base = nullptr;

        Plic(driver::DriverBase::DevRes res, driver::intc_t identifier,
             driver::hwirq_t srccnt, std::vector<Context> contexts,
             volatile char *base) noexcept;

        template <typename T>
        static volatile T *resolve_base(volatile char *base,
                                        size_t offset) noexcept {
            return reinterpret_cast<volatile T *>(base + offset);
        }

        [[nodiscard]]
        Result<void> initialize(driver::IrqDomain *domain) noexcept;

    public:
        [[nodiscard]]
        static Result<util::owner<Plic *>> create(
            driver::DriverBase::DevRes devres, driver::intc_t identifier,
            driver::hwirq_t srccnt, std::vector<Context> ctxs) noexcept;

        [[nodiscard]]
        Result<void> attach_to_parent_domain(
            driver::IrqManager &irqman,
            driver::IrqDomain &self_domain) noexcept override;
    };
}  // namespace riscv
