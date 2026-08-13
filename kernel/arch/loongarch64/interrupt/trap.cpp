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
#include <arch/loongarch64/valdef.h>
#include <arch/loongarch64/interrupt/frame.h>
#include <log.h>
#include <sustcore/addrspace.h>
#include <trap/dispatcher.h>

extern "C" void _loongarch_early_trap_entry();
extern "C" void _loongarch_runtime_trap_entry();
extern "C" void _loongarch_kernel_tlb_refill();

namespace loongarch64::hal {
    namespace {
        [[nodiscard]] const char *exception_name(xlen_t code) noexcept {
            constexpr const char *NAMES[] = {
                "中断",
                "load 页无效",
                "store 页无效",
                "取指页无效",
                "页修改",
                "页不可读",
                "页不可执行",
                "页特权等级不合规",
                "地址错误",
                "地址未对齐",
                "边界检查错误",
                "系统调用",
                "断点",
                "指令不存在",
                "指令特权等级错误",
                "浮点指令未使能",
                "LSX 指令未使能",
                "LASX 指令未使能",
                "浮点异常",
                "监测点异常",
                "二进制翻译扩展未使能",
                "二进制翻译异常",
                "客户机敏感特权资源异常",
                "虚拟机监控调用",
                "客户机 CSR 修改异常",
            };
            return code < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[code] : "未知异常";
        }

        [[nodiscard]] const char *interrupt_name(xlen_t code) noexcept {
            switch (code) {
                case 0:  return "软件中断 0";
                case 1:  return "软件中断 1";
                case 2:  return "硬件中断 0";
                case 3:  return "硬件中断 1";
                case 4:  return "硬件中断 2";
                case 5:  return "硬件中断 3";
                case 6:  return "硬件中断 4";
                case 7:  return "硬件中断 5";
                case 8:  return "硬件中断 6";
                case 9:  return "硬件中断 7";
                case 10: return "性能计数器溢出中断";
                case 11: return "定时器中断";
                case 12: return "IPI 中断";
                default: return "未知中断";
            }
        }

        [[nodiscard]] xlen_t exception_subcode(xlen_t estat) noexcept {
            return (estat >> ESTAT_ESUBCODE_SHIFT) & ESTAT_ESUBCODE_MASK;
        }

    }  // namespace

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
        memory::FaultAccess access = memory::FaultAccess::NONE;
        if (kind == TrapKind::SYNCHRONOUS) {
            if (code == ECODE_PIL)
                access = memory::FaultAccess::READ;
            else if (code == ECODE_PIS)
                access = memory::FaultAccess::WRITE;
            else if (code == ECODE_PIF)
                access = memory::FaultAccess::EXECUTE;
        }
        return TrapInfo{kind, frame.estat, code, frame.badv, from_user(frame), access};
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

    bool is_user_syscall(const TrapInfo &info) noexcept {
        return info.user && info.kind == TrapKind::SYNCHRONOUS && info.code == ECODE_SYS;
    }

    bool is_page_fault(const TrapInfo &info) noexcept {
        return info.kind == TrapKind::SYNCHRONOUS &&
               (info.code == ECODE_PIL || info.code == ECODE_PIS || info.code == ECODE_PIF);
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

    xlen_t trap_subcode(const TrapInfo &info) noexcept {
        return exception_subcode(info.raw_cause);
    }

    void initialize_user_frame(TrapFrame &frame, addr_t entry, addr_t stack_pointer,
                               addr_t argument) noexcept {
        frame      = TrapFrame{};
        frame.era  = entry;
        frame.sp   = stack_pointer;
        frame.a0   = argument;
        frame.prmd = PRMD_USER;
    }

    void wait_for_interrupt() noexcept {
        asm volatile("idle 0" ::: "memory");
    }

    extern "C" [[noreturn]] void arch_early_trap_handler(xlen_t estat, addr_t era, addr_t badv) {
        const auto ecode = (estat >> ESTAT_ECODE_SHIFT) & ESTAT_ECODE_MASK;
        kernel::log::panic(
            "早期 LoongArch 异常: name={}, ecode={}, esubcode={}, estat={:#x}, "
            "era={:#x}, badv={:#x}",
            exception_name(ecode), ecode, exception_subcode(estat), estat, era, badv);
    }

    extern "C" void arch_runtime_trap_handler(TrapFrame *frame) {
        kernel::trap::dispatch(*frame);
    }
}  // namespace loongarch64::hal
