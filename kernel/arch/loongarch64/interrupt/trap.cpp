/**
 * @file trap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch 陷阱与通用中断模型适配
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/csr.h>
#include <arch/interrupt.h>
#include <arch/loongarch64/csrdef.h>
#include <arch/loongarch64/interrupt/frame.h>
#include <log.h>
#include <sustcore/addrspace.h>

extern "C" void _loongarch_early_trap_entry();
extern "C" void _loongarch_runtime_trap_entry();
extern "C" void _loongarch_kernel_tlb_refill();

namespace loongarch64::hal {
    interrupt_guard::interrupt_guard() noexcept
        // 保存旧中断位并原子清除，使嵌套 guard 只由最外层恢复中断状态。
        : previous_(csr::clear_bits<csr::CSR::CRMD>(CRMD_IE)) {}

    interrupt_guard::~interrupt_guard() noexcept {
        if ((previous_ & CRMD_IE) != 0) {
            (void)csr::set_bits<csr::CSR::CRMD>(CRMD_IE);
        }
    }

    void disable_interrupts() noexcept {
        (void)csr::clear_bits<csr::CSR::CRMD>(CRMD_IE);
    }

    void install_exception_vectors() noexcept {
        csr::write<csr::CSR::EENTRY>(reinterpret_cast<addr_t>(&_loongarch_early_trap_entry));
        csr::write<csr::CSR::TLBRENTRY>(reinterpret_cast<addr_t>(&_loongarch_kernel_tlb_refill) -
                                        KVA_START);
    }

    void enable_interrupts() noexcept {
        (void)csr::set_bits<csr::CSR::CRMD>(CRMD_IE);
    }

    bool interrupts_enabled() noexcept {
        return (csr::read<csr::CSR::CRMD>() & CRMD_IE) != 0;
    }

    void install_early_exception_vectors() noexcept {
        install_exception_vectors();
    }

    void install_runtime_exception_vectors() noexcept {
        csr::write<csr::CSR::EENTRY>(reinterpret_cast<addr_t>(&_loongarch_runtime_trap_entry));
    }

    TrapInfo decode_trap(const TrapFrame &frame) noexcept {
        const auto ecode   = (frame.estat >> ESTAT_ECODE_SHIFT) & ESTAT_ECODE_MASK;
        const auto pending = frame.estat & ESTAT_IS_MASK;
        TrapKind kind      = TrapKind::SYNCHRONOUS;
        xlen_t code        = ecode;
        if (ecode == ECODE_INT) {
            if ((pending & ECFG_TIMER) != 0) {
                kind = TrapKind::TIMER;
                code = INT_TIMER;
            } else if ((pending & (1U << 12)) != 0) {
                kind = TrapKind::SOFTWARE;
                code = 12;
            } else {
                kind = TrapKind::EXTERNAL;
                code = pending == 0 ? 0 : static_cast<xlen_t>(__builtin_ctzll(pending));
            }
        }
        return TrapInfo{kind, frame.estat, code, frame.badv, from_user(frame)};
    }

    bool from_user(const TrapFrame &frame) noexcept {
        return (frame.prmd & PRMD_PPLV_MASK) == PLV_USER;
    }

    addr_t program_counter(const TrapFrame &frame) noexcept {
        return static_cast<addr_t>(frame.era);
    }

    void set_program_counter(TrapFrame &frame, addr_t value) noexcept {
        frame.era = static_cast<xlen_t>(value);
    }

    void wait_for_interrupt() noexcept {
        asm volatile("idle 0" ::: "memory");
    }

    extern "C" [[noreturn]] void arch_early_trap_handler(xlen_t estat, addr_t era, addr_t badv) {
        kernel::log::panic("早期 LoongArch 陷阱: estat={}, era={}, badv={}", estat, era, badv);
    }

    extern "C" [[noreturn]] void arch_runtime_trap_handler(TrapFrame *) {
        kernel::log::panic("M0 启动阶段不支持运行时陷阱");
    }
}  // namespace loongarch64::hal
