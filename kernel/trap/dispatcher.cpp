/**
 * @file dispatcher.cpp
 * @brief 通用 syscall、页故障修复与 fatal trap 诊断。
 */

#include <arch/interrupt.h>
#include <arch/paging_traits.h>
#include <log.h>
#include <memory/virtual/client/client_space.h>
#include <obj/process.h>
#include <scheduler/scheduler.h>
#include <syscall.h>
#include <trap/dispatcher.h>

namespace kernel::trap {
    namespace {
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

            const auto number = hal::syscall_number(frame);
            if (number == syscall::EC_WRITE_SYSCALL) {
                auto result = syscall::ec_write(
                    reinterpret_cast<const char *>(hal::syscall_argument(frame, 0)),
                    static_cast<size_t>(hal::syscall_argument(frame, 1)));
                hal::set_syscall_result(
                    frame, result ? static_cast<xlen_t>(*result) : static_cast<xlen_t>(-1));
                hal::advance_syscall(frame);
                return true;
            }
            if (number == syscall::YIELD_SYSCALL) {
                hal::set_syscall_result(frame, 0);
                hal::advance_syscall(frame);
                syscall::yield();
                return true;
            }
            kernel::log::panic("未知系统调用: number={}, pc={:#x}", number,
                               hal::program_counter(frame));
        }

        [[nodiscard]] bool dispatch_user_page_fault(hal::TrapFrame &frame,
                                                    const hal::TrapInfo &info) noexcept {
            if (!info.user || !hal::is_page_fault(info))
                return false;
            auto *current = scheduler::instance().current();
            if (current == nullptr || current->process().kernel() ||
                current->process().address_space() == nullptr)
                return false;
            auto address = VirAddr::try_from(info.bad_address);
            if (!address)
                return false;
            auto handled =
                current->process().address_space()->handle_page_fault(*address, info.access);
            if (handled)
                return true;
            kernel::log::panic(
                "用户缺页处理失败: name={}, access={}, pc={:#x}, bad={:#x}, code={}, "
                "subcode={}, error={}",
                hal::trap_name(info), access_name(info.access), hal::program_counter(frame),
                info.bad_address, info.code, hal::trap_subcode(info),
                tay::to_string(handled.error()));
        }

        [[nodiscard]] bool repair_kernel_mapping(const hal::TrapInfo &info) noexcept {
            if (info.user || !hal::is_page_fault(info) ||
                !hal::PageTableOps::canonical(info.bad_address))
                return false;
            if constexpr (!hal::PageTableOps::SHARES_HIGH_ROOT)
                return false;
            auto address = HvaAddr::try_from(info.bad_address);
            auto *client = memory::active_client_space();
            if (!address || client == nullptr || client->binding().role != memory::RootRole::CLIENT)
                return false;
            auto repaired = client->repair_missing_borrowed_kernel_slot(*address);
            if (!repaired)
                kernel::log::panic("高半区根项所有权损坏: {}", static_cast<int>(repaired.error()));
            if (*repaired == memory::BorrowedSlotRepair::REPAIRED)
                return true;
            if (*repaired == memory::BorrowedSlotRepair::GLOBAL_SLOT_ABSENT)
                kernel::log::panic("内核高半区地址没有全局映射: {:#x}", info.bad_address);
            return false;
        }
    }  // namespace

    void dispatch(hal::TrapFrame &frame) noexcept {
        const auto info = hal::decode_trap(frame);
        if (dispatch_syscall(frame, info) || dispatch_user_page_fault(frame, info) ||
            repair_kernel_mapping(info))
            return;
        kernel::log::panic(
            "未处理的 trap: name={}, kind={}, mode={}, code={}, subcode={}, raw={:#x}, "
            "access={}, pc={:#x}, bad={:#x}",
            hal::trap_name(info), kind_name(info.kind), info.user ? "user" : "kernel", info.code,
            hal::trap_subcode(info), info.raw_cause, access_name(info.access),
            hal::program_counter(frame), info.bad_address);
    }
}  // namespace kernel::trap
