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
#include <arch/riscv64/interrupt/frame.h>
#include <arch/riscv64/namespace.h>
#include <log.h>
#include <trap/dispatcher.h>

extern "C" void _riscv_early_trap_entry();
extern "C" void _riscv_runtime_trap_entry();

namespace riscv64::hal {
    namespace {
        [[nodiscard]] const char *exception_name(xlen_t code) noexcept {
            constexpr const char *NAMES[] = {
                "指令地址未对齐", "指令访问错误",   "非法指令",       "断点",
                "加载地址未对齐", "加载访问错误",   "存储地址未对齐", "存储访问错误",
                "用户态环境调用", "监管态环境调用", "保留",           "保留",
                "取指缺页",       "加载缺页",       "保留",           "存储缺页",
                "保留",           "保留",           "软件检查异常",   "硬件错误",
            };
            return code < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[code] : "未知异常";
        }

        [[nodiscard]] const char *interrupt_name(xlen_t code) noexcept {
            switch (code) {
                case 1:  return "监管态软件中断";
                case 5:  return "监管态定时器中断";
                case 9:  return "监管态外部中断";
                default: return "未知中断";
            }
        }

    }  // namespace

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
        memory::FaultAccess access = memory::FaultAccess::NONE;
        if (kind == TrapKind::SYNCHRONOUS) {
            if (code == 12)
                access = memory::FaultAccess::EXECUTE;
            else if (code == 13)
                access = memory::FaultAccess::READ;
            else if (code == 15)
                access = memory::FaultAccess::WRITE;
        }
        return TrapInfo{.kind        = kind,
                        .raw_cause   = frame.scause,
                        .code        = code,
                        .bad_address = frame.stval,
                        .user        = from_user(frame),
                        .access      = access};
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

    bool is_user_syscall(const TrapInfo &info) noexcept {
        return info.user && info.kind == TrapKind::SYNCHRONOUS && info.code == 8;
    }

    bool is_page_fault(const TrapInfo &info) noexcept {
        return info.kind == TrapKind::SYNCHRONOUS &&
               (info.code == 12 || info.code == 13 || info.code == 15);
    }

    xlen_t syscall_number(const TrapFrame &frame) noexcept {
        return frame.a7;
    }

    xlen_t syscall_argument(const TrapFrame &frame, size_t index) noexcept {
        switch (index) {
            case 0:  return frame.a0;
            case 1:  return frame.a1;
            default: return 0;
        }
    }

    void set_syscall_result(TrapFrame &frame, xlen_t value) noexcept {
        frame.a0 = value;
    }

    void advance_syscall(TrapFrame &frame) noexcept {
        set_program_counter(frame, program_counter(frame) + 4);
    }

    const char *trap_name(const TrapInfo &info) noexcept {
        return info.kind == TrapKind::SYNCHRONOUS ? exception_name(info.code)
                                                  : interrupt_name(info.code);
    }

    xlen_t trap_subcode(const TrapInfo &) noexcept {
        return 0;
    }

    void initialize_user_frame(TrapFrame &frame, addr_t entry, addr_t stack_pointer,
                               addr_t argument) noexcept {
        frame         = TrapFrame{};
        frame.sepc    = entry;
        frame.sp      = stack_pointer;
        frame.a0      = argument;
        frame.sstatus = 1U << 5;
    }

    void wait_for_interrupt() noexcept {
        asm volatile("wfi" ::: "memory");
    }
    extern "C" [[noreturn]] void arch_early_trap_handler(xlen_t cause, addr_t pc, addr_t bad) {
        constexpr xlen_t INTERRUPT_BIT = xlen_t{1} << 63;
        const bool interrupt           = (cause & INTERRUPT_BIT) != 0;
        const xlen_t code              = cause & ~INTERRUPT_BIT;
        kernel::log::panic("早期 RISC-V {}: name={}, code={}, raw={:#x}, pc={:#x}, stval={:#x}",
                           interrupt ? "中断" : "异常",
                           interrupt ? interrupt_name(code) : exception_name(code), code, cause, pc,
                           bad);
    }

    extern "C" void arch_runtime_trap_handler(TrapFrame *frame) {
        kernel::trap::dispatch(*frame);
    }
}  // namespace riscv64::hal
