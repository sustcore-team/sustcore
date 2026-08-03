/**
 * @file trap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V 陷阱与通用中断模型适配
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/csr.h>
#include <arch/interrupt.h>
#include <arch/paging_traits.h>
#include <arch/riscv64/interrupt/frame.h>
#include <log.h>
#include <memory/virtual/client/client_space.h>

extern "C" void _riscv_early_trap_entry();
extern "C" void _riscv_runtime_trap_entry();

namespace riscv64::hal {
    interrupt_guard::interrupt_guard() noexcept
        // 保存旧中断位并原子清除，使嵌套 guard 只由最外层恢复中断状态。
        : previous_(csr::clear_bits<csr::CSR::SSTATUS>(1U << 1)) {}

    interrupt_guard::~interrupt_guard() noexcept {
        if ((previous_ & (1U << 1)) != 0) {
            (void)csr::set_bits<csr::CSR::SSTATUS>(1U << 1);
        }
    }

    void disable_interrupts() noexcept {
        (void)csr::clear_bits<csr::CSR::SSTATUS>(1U << 1);
    }

    void enable_interrupts() noexcept {
        (void)csr::set_bits<csr::CSR::SSTATUS>(1U << 1);
    }

    bool interrupts_enabled() noexcept {
        return (csr::read<csr::CSR::SSTATUS>() & (1U << 1)) != 0;
    }

    void install_early_exception_vectors() noexcept {
        csr::write<csr::CSR::SSCRATCH>(0);
        csr::write<csr::CSR::STVEC>(reinterpret_cast<addr_t>(&_riscv_early_trap_entry));
    }

    void install_runtime_exception_vectors() noexcept {
        csr::write<csr::CSR::SSCRATCH>(0);
        csr::write<csr::CSR::STVEC>(reinterpret_cast<addr_t>(&_riscv_runtime_trap_entry));
    }

    TrapInfo decode_trap(const TrapFrame &frame) noexcept {
        constexpr xlen_t INTERRUPT_BIT = xlen_t{1} << 63;
        const bool is_interrupt        = (frame.scause & INTERRUPT_BIT) != 0;
        const auto code                = frame.scause & ~INTERRUPT_BIT;
        TrapKind kind                  = TrapKind::SYNCHRONOUS;
        if (is_interrupt) {
            if (code == 5)
                kind = TrapKind::TIMER;
            else if (code == 1)
                kind = TrapKind::SOFTWARE;
            else
                kind = TrapKind::EXTERNAL;
        }
        return TrapInfo{kind, frame.scause, code, frame.stval, from_user(frame)};
    }

    bool from_user(const TrapFrame &frame) noexcept {
        return (frame.sstatus & (1U << 8)) == 0;
    }

    addr_t program_counter(const TrapFrame &frame) noexcept {
        return static_cast<addr_t>(frame.sepc);
    }

    void set_program_counter(TrapFrame &frame, addr_t value) noexcept {
        frame.sepc = static_cast<xlen_t>(value);
    }

    void wait_for_interrupt() noexcept {
        asm volatile("wfi" ::: "memory");
    }
    extern "C" [[noreturn]] void arch_early_trap_handler(xlen_t cause, addr_t pc, addr_t bad) {
        kernel::log::panic("早期 RISC-V 陷阱: 异常原因={}, PC={}, stval={}", cause, pc, bad);
    }

    extern "C" void arch_runtime_trap_handler(TrapFrame *frame) {
        const TrapInfo info   = decode_trap(*frame);
        const bool page_fault = info.kind == TrapKind::SYNCHRONOUS &&
                                (info.code == 12 || info.code == 13 || info.code == 15);
        if (page_fault && !info.user && PageTableOps::canonical(info.bad_address)) {
            auto address = HvaAddr::try_from(info.bad_address);
            auto *client = memory::active_client_space();
            if (address && client != nullptr && client->binding().role == memory::RootRole::CLIENT)
            {
                auto repaired = client->repair_missing_borrowed_kernel_slot(*address);
                if (!repaired)
                    kernel::log::panic("RISC-V 高半区根项所有权损坏: {}",
                                       static_cast<int>(repaired.error()));
                if (*repaired == memory::BorrowedSlotRepair::REPAIRED)
                    return;
                if (*repaired == memory::BorrowedSlotRepair::GLOBAL_SLOT_ABSENT)
                    kernel::log::panic("RISC-V 内核高半区地址没有全局映射: {}", info.bad_address);
            }
        }
        kernel::log::panic("未处理的 RISC-V 运行时陷阱: cause={}, pc={}, bad={}", info.raw_cause,
                           program_counter(*frame), info.bad_address);
    }
}  // namespace riscv64::hal
