/**
 * @file early_console.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核早期控制台架构接口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/namespace.h>

#include <concepts>

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    class EarlyConsole;

    template <class T>
    concept EarlyConsoleTraits = requires(T &console, char ch) {
        {
            console.putc(ch)
        } noexcept -> std::same_as<void>;
        {
            console.halt()
        } noexcept -> std::same_as<void>;
    };

    /**
     * @brief 最早期架构控制台的唯一实例类型。
     */
    class EarlyConsole final {
    public:
        void putc(char ch) noexcept;
        [[noreturn]] void halt() noexcept;

        EarlyConsole(const EarlyConsole &)            = delete;
        EarlyConsole &operator=(const EarlyConsole &) = delete;
        EarlyConsole(EarlyConsole &&)                 = delete;
        EarlyConsole &operator=(EarlyConsole &&)      = delete;

    private:
        constexpr EarlyConsole() noexcept = default;

        static EarlyConsole instance_;

        friend EarlyConsole &early_console() noexcept;
    };

    static_assert(EarlyConsoleTraits<EarlyConsole>);

    /** @brief 获取无需运行期初始化的早期控制台唯一实例。 */
    EarlyConsole &early_console() noexcept;
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
