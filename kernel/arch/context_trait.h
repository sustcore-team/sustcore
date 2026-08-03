/**
 * @file context_trait.h
 * @brief 调度上下文架构实现的公共编译期契约
 */

#pragma once

#include <arch/namespace.h>
#include <tay/bits.h>

#include <concepts>
#include <cstddef>
#include <type_traits>

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    constexpr size_t CONTEXT_ALIGNMENT = 16;

    /**
     * @brief 调度器可依赖的最小上下文接口。
     *
     * 具体寄存器集合和汇编偏移由各架构拥有；公共代码只通过 ra 和 sp 访问器构造
     * 首次运行的上下文。
     */
    template <class T>
    concept ContextTrait = std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T> &&
                           requires(T &context, const T &constant_context) {
                               requires(alignof(T) >= CONTEXT_ALIGNMENT);
                               requires(sizeof(T) % CONTEXT_ALIGNMENT == 0);
                               requires(T::SIZE_BYTES == sizeof(T));
                               {
                                   context.ra()
                               } noexcept -> std::same_as<addr_t &>;
                               {
                                   constant_context.ra()
                               } noexcept -> std::same_as<const addr_t &>;
                               {
                                   context.sp()
                               } noexcept -> std::same_as<addr_t &>;
                               {
                                   constant_context.sp()
                               } noexcept -> std::same_as<const addr_t &>;
                           };
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
