/**
 * @file dispatcher.cpp
 * @brief 通用 syscall、页故障修复与 fatal trap 诊断。
 */

#include <arch/interrupt.h>
#include <arch/paging_traits.h>
#include <arch/smp.h>
#include <device/interrupt.h>
#include <log.h>
#include <memory/virtual/user/vm.h>
#include <obj/process.h>
#include <scheduler/scheduler.h>
#include <smp/ipi.h>
#include <syscall.h>
#include <trap/dispatcher.h>

namespace kernel::trap {
    namespace detail::dispatcher {
        [[nodiscard]] const char *access_name(memory::FaultAccess access) noexcept {
            switch (access) {
                case memory::FaultAccess::READ:    return "read";
                case memory::FaultAccess::WRITE:   return "write";
                case memory::FaultAccess::EXECUTE: return "execute";
                case memory::FaultAccess::NONE:    return "none";
            }
            return "unknown";
        }

        [[nodiscard]] const char *kind_name(hal::TrapKind kind) noexcept {
            switch (kind) {
                case hal::TrapKind::SYNCHRONOUS: return "exception";
                case hal::TrapKind::TIMER:       return "timer";
                case hal::TrapKind::SOFTWARE:    return "software";
                case hal::TrapKind::EXTERNAL:    return "external";
            }
            return "unknown";
        }

        [[nodiscard]] bool dispatch_syscall(hal::TrapFrame &frame,
                                            const hal::TrapInfo &info) noexcept {
            if (!hal::is_user_syscall(info))
                return false;

            const auto number = hal::syscall_nr(frame);
            if (number == syscall::EC_WRITE_SYSCALL) {
                auto result =
                    syscall::ec_write(reinterpret_cast<const char *>(hal::syscall_arg(frame, 0)),
                                      static_cast<size_t>(hal::syscall_arg(frame, 1)));
                hal::set_syscall_ret(
                    frame, result ? static_cast<xlen_t>(*result) : static_cast<xlen_t>(-1));
                hal::advance_syscall(frame);
                return true;
            }
            if (number == syscall::YIELD_SYSCALL) {
                hal::set_syscall_ret(frame, 0);
                hal::advance_syscall(frame);
                syscall::yield();
                return true;
            }
            kernel::log::panic("未知系统调用: number={}, pc={:#x}", number, hal::pc(frame));
        }

        [[nodiscard]] bool handle_user_fault(hal::TrapFrame &frame,
                                               const hal::TrapInfo &info) noexcept {
            if (!info.user || !hal::is_page_fault(info))
                return false;
            auto *current = scheduler::current();
            if (current == nullptr || current->process().kernel() ||
                current->process().addr_space() == nullptr)
                return false;
            auto address = VirAddr::try_from(info.bad_address);
            if (!address)
                return false;
            auto handled = current->process().addr_space()->handle_fault(*address, info.access);
            if (handled)
                return true;
            kernel::log::panic(
                "用户缺页处理失败: name={}, access={}, pc={:#x}, bad={:#x}, code={}, "
                "subcode={}, error={}",
                hal::trap_name(info), access_name(info.access), hal::pc(frame), info.bad_address,
                info.code, hal::trap_subcode(info), handled.error());
        }

        [[nodiscard]] bool fix_kernel_map(const hal::TrapInfo &info) noexcept {
            if (info.user || !hal::is_page_fault(info) || !hal::PtOps::canonical(info.bad_address))
                return false;
            if constexpr (!hal::PtOps::SHARES_HIGH_ROOT)
                return false;
            auto address = HvaAddr::try_from(info.bad_address);
            auto *client = memory::active_user_vm();
            if (!address || client == nullptr || client->binding().role != memory::RootRole::CLIENT)
                return false;
            auto repaired = client->fix_borrowed_slot(*address);
            if (!repaired)
                kernel::log::panic("高半区根项所有权损坏: {}", repaired.error());
            if (*repaired == memory::BorrowedSlotFix::REPAIRED)
                return true;
            if (*repaired == memory::BorrowedSlotFix::GLOBAL_SLOT_ABSENT)
                kernel::log::panic("内核高半区地址没有全局映射: {:#x}", info.bad_address);
            return false;
        }
    }  // namespace detail::dispatcher

    void dispatch(hal::TrapFrame &frame) noexcept {
        const auto info = hal::decode_trap(frame);
        bool handled    = detail::dispatcher::dispatch_syscall(frame, info) ||
                       detail::dispatcher::handle_user_fault(frame, info) ||
                       detail::dispatcher::fix_kernel_map(info);
        if (!handled && info.kind == hal::TrapKind::SOFTWARE) {
            {
                cpu::irq_context_guard interrupt_context;
                hal::ack_ipi();
                smp::dispatch_ipi();
            }
            handled = true;
        }
        if (!handled && info.kind != hal::TrapKind::SYNCHRONOUS)
            handled =
                device::interrupt::dispatch(info) == device::interrupt::DispatchResult::HANDLED;

        if (!handled)
            kernel::log::panic(
                "未处理的 trap: name={}, kind={}, mode={}, code={}, subcode={}, raw={:#x}, "
                "access={}, pc={:#x}, bad={:#x}",
                hal::trap_name(info), detail::dispatcher::kind_name(info.kind),
                info.user ? "user" : "kernel", info.code, hal::trap_subcode(info), info.raw_cause,
                detail::dispatcher::access_name(info.access), hal::pc(frame), info.bad_address);

        // 所有可恢复 trap 都在状态确认完成后经过同一调度安全点；IRQ 与唤醒路径只发布
        // RunQueueFlags::NEED_RESCHED，是否重新选择和切换完全由 SchedCore 决定。
        scheduler::local().schedule();
    }
}  // namespace kernel::trap
