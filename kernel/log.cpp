/**
 * @file log.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核诊断日志与致命错误输出
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/early_console.h>
#include <arch/interrupt.h>
#include <arch/smp.h>
#include <cpu/topology.h>
#include <log.h>
#include <smp/ipi.h>

#include <atomic>
#include <cstddef>

namespace kernel::log {
    namespace detail::panic {
        constinit Logger logger;
        constinit tay::ticket_spinlock logger_lock;
        constinit std::atomic<u32_t> panic_owner{cpu::INVALID_CPU};

        int write_bytes(const char *data, size_t sz) noexcept {
            for (size_t i = 0; i < sz; ++i) {
                putc(data[i]);
            }
            return static_cast<int>(sz);
        }

        void stop_other_cpus() noexcept {
            if (!cpu::topology().ready())
                return;
            const auto self = cpu::current_id();
            // STARTED 集合包含尚未完成 READY/ONLINE 握手的 AP；panic 也必须停止这些
            // bring-up CPU，避免 BSP 在 early SMP 阶段 halt 后留下继续执行的 AP。
            cpu::topology().snapshot().started.for_each([self](cpu::CpuId target) noexcept {
                if (target == self)
                    return;
                const auto requested = smp::request(target, smp::IpiReason::STOP);
                if (requested)
                    return;
                // request() 已将 STOP 留在 mailbox；直接重试硬件门铃，避免 pending 位合并
                // 使第二次 request() 被当作 no-op。失败时仅输出紧急诊断，不能再取得 logger 锁。
                if (hal::send_ipi(cpu::topology().hw_id(target)))
                    return;
                static constexpr char MESSAGE[] = "\nPANIC: STOP IPI failed\n";
                for (const char ch : MESSAGE)
                    if (ch != '\0')
                        putc(ch);
            });
        }
    }  // namespace detail::panic

    int LogOutput::operator()(const char *data, size_t sz) const noexcept {
        return detail::panic::write_bytes(data, sz);
    }

    LoggerGuard::LoggerGuard(Logger &logger, tay::ticket_spinlock &lock, bool try_lock) noexcept
        : logger_(logger), lock_(lock, tay::defer_lock) {
        if (try_lock)
            static_cast<void>(lock_.try_lock());
        else
            lock_.lock();
    }

    LoggerGuard::~LoggerGuard() noexcept = default;

    LoggerGuard global() noexcept {
        return LoggerGuard(detail::panic::logger, detail::panic::logger_lock, false);
    }

    LoggerGuard try_global() noexcept {
        return LoggerGuard(detail::panic::logger, detail::panic::logger_lock, true);
    }

    void putc(char ch) noexcept {
        hal::early_console().putc(ch);
    }

    [[noreturn]] void halt() noexcept {
        hal::early_console().halt();
    }

    bool claim_panic_owner() noexcept {
        u32_t expected   = cpu::INVALID_CPU;
        const u32_t self = cpu::current_id().value;
        if (!detail::panic::panic_owner.compare_exchange_strong(
                expected, self, std::memory_order_acq_rel, std::memory_order_acquire))
            return false;
        hal::cli();
        detail::panic::stop_other_cpus();
        return true;
    }

    [[noreturn]] void emergency_halt() noexcept {
        static constexpr char MESSAGE[] = "\nKERNEL PANIC\n";
        for (const char ch : MESSAGE)
            if (ch != '\0')
                putc(ch);
        hal::cli();
        halt();
    }

    [[noreturn]] void ap_halt() noexcept {
        hal::cli();
        halt();
    }
}  // namespace kernel::log

[[noreturn]] void tay::panic(const char *message) noexcept {
    kernel::log::panic("{}", message);
}
