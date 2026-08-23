/**
 * @file log.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核诊断日志与致命错误报告接口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <feature/attributes.h>
#include <synchronized.h>
#include <tay/logger.h>
#include <tay/spinlock.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace kernel::log {
    /** @brief 将格式化后的日志字节写入架构早期控制台。 */
    struct LogOutput {
        /**
         * @brief 输出一段完整的日志字节序列。
         * @param data 待输出缓冲区；在调用期间必须有效。
         * @param sz 缓冲区字节数。
         * @return 实际提交给控制台的字节数。
         */
        int operator()(const char *data, size_t sz) const noexcept;
    };

    using Logger = tay::logger<LogOutput, tay::log_level::DEBUG, 256>;

    /**
     * @brief Logger 的 IRQ-safe 临界区所有者。
     * @note panic 使用 try_global()，因此不会为格式化日志等待另一个 CPU 的普通输出锁。
     */
    class LoggerGuard final {
    public:
        LoggerGuard(Logger &logger, tay::ticket_spinlock &lock, bool try_lock) noexcept;
        ~LoggerGuard() noexcept;

        LoggerGuard(const LoggerGuard &)            = delete;
        LoggerGuard &operator=(const LoggerGuard &) = delete;
        LoggerGuard(LoggerGuard &&)                 = delete;
        LoggerGuard &operator=(LoggerGuard &&)      = delete;

        [[nodiscard]] explicit operator bool() const noexcept {
            return lock_.owns_lock();
        }
        [[nodiscard]] Logger *operator->() noexcept {
            return &logger_;
        }

    private:
        Logger &logger_;
        hal::preempt_guard preemption_guard_;
        hal::irq_guard interrupt_guard_;
        tay::unique_lock<tay::ticket_spinlock> lock_;
    };

    /**
     * @brief 获取全局内核 logger 的 IRQ-safe 锁定引用。
     * @return 持有 logger ticket spinlock、抢占保护和本地中断保护的临时引用。
     * @note 锁保持到返回对象析构；调用者不得跨调度点保存该引用。
     */
    [[nodiscard]] LoggerGuard global() noexcept;
    [[nodiscard]] LoggerGuard try_global() noexcept;

    /** @brief 不经过格式化器，直接向架构控制台输出一个字符。 */
    void putc(char ch) noexcept;

    /** @brief 关闭本地中断并永久停驻当前 CPU。 */
    [[noreturn]] void halt() noexcept;
    [[nodiscard]] bool claim_panic_owner() noexcept;
    [[noreturn]] void emergency_halt() noexcept;
    [[noreturn]] void ap_halt() noexcept;

    /**
     * @brief 记录 DEBUG 级别的格式化消息。
     * @note 调用期间会禁止抢占、关闭本地中断并串行化全局日志输出。
     */
    template <class... Args>
    void debug(tay::logger_format_string<std::type_identity_t<Args>...> format,
               Args &&...args) noexcept {
        (void)global()->debug(format, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录 INFO 级别的格式化消息。
     * @note 调用期间会禁止抢占、关闭本地中断并串行化全局日志输出。
     */
    template <class... Args>
    void info(tay::logger_format_string<std::type_identity_t<Args>...> format,
              Args &&...args) noexcept {
        (void)global()->info(format, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录 WARN 级别的格式化消息。
     * @note 调用期间会禁止抢占、关闭本地中断并串行化全局日志输出。
     */
    template <class... Args>
    void warn(tay::logger_format_string<std::type_identity_t<Args>...> format,
              Args &&...args) noexcept {
        (void)global()->warn(format, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录 ERROR 级别的格式化消息。
     * @note 调用期间会禁止抢占、关闭本地中断并串行化全局日志输出。
     */
    template <class... Args>
    void error(tay::logger_format_string<std::type_identity_t<Args>...> format,
               Args &&...args) noexcept {
        (void)global()->error(format, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录 FATAL 级别消息并永久停驻当前 CPU。
     * @note 该函数不返回；它不会尝试恢复当前内核操作。
     */
    template <class... Args>
    [[noreturn]] void panic(tay::logger_format_string<std::type_identity_t<Args>...> format,
                            Args &&...args) noexcept {
        if (!claim_panic_owner())
            ap_halt();
        auto logger = try_global();
        if (logger)
            (void)logger->fatal(format, std::forward<Args>(args)...);
        emergency_halt();
    }
}  // namespace kernel::log

namespace kernel {
    /** @brief `kernel::log::global()` 的兼容入口。 */
    static __ATTR_ALWAYS_INLINE__ auto logger() noexcept {
        return kernel::log::global();
    }
}  // namespace kernel
