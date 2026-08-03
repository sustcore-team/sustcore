/**
 * @file paging.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页表架构策略的编译期接口约束
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/namespace.h>
#include <memory/virtual/page_flags.h>
#include <memory/virtual/root_binding.h>
#include <sustcore/addr.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace memory {
    class KernelSpace;
}

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    /**
     * @brief 通用页表 walker 对架构 traits 的最小静态接口要求。
     *
     * 具体架构只需满足此 concept；页表级数、PTE 位布局、根寄存器和 TLB 指令均不进入
     * 架构无关 walker 的实现。
     */
    template <class Ops>
    concept PageTableTraits = requires(
        typename Ops::EntryType entry, typename Ops::EntryType *entry_ptr, PhyAddr physical,
        addr_t address, size_t level, memory::PageFlags flags, const memory::RootBinding &binding) {
        typename Ops::EntryType;
        requires std::is_unsigned_v<typename Ops::EntryType>;
        requires(Ops::ENTRIES_PER_TABLE > 0);
        requires(Ops::ENTRIES_PER_TABLE * sizeof(typename Ops::EntryType) <= PAGE_SIZE);
        requires(Ops::TOP_LEVEL > 0);
        {
            Ops::index_at(address, level)
        } noexcept -> std::convertible_to<size_t>;
        {
            Ops::table(physical)
        } noexcept -> std::same_as<typename Ops::EntryType *>;
        {
            Ops::load_entry(entry_ptr)
        } noexcept -> std::same_as<typename Ops::EntryType>;
        {
            Ops::store_leaf(entry_ptr, entry)
        } noexcept;
        {
            Ops::publish_table(entry_ptr, entry)
        } noexcept;
        {
            Ops::present(entry)
        } noexcept -> std::convertible_to<bool>;
        {
            Ops::leaf(entry)
        } noexcept -> std::convertible_to<bool>;
        {
            Ops::next_table(entry)
        } noexcept -> std::same_as<PhyAddr>;
        {
            Ops::make_table(physical)
        } noexcept -> std::same_as<typename Ops::EntryType>;
        {
            Ops::make_leaf(physical, flags)
        } noexcept -> std::same_as<tay::expected<typename Ops::EntryType, tay::error_code>>;
        {
            Ops::decode_flags(entry)
        } noexcept -> std::same_as<memory::PageFlags>;
        {
            Ops::leaf_physical(entry, address, level)
        } noexcept -> std::same_as<PhyAddr>;
        {
            Ops::canonical(address)
        } noexcept -> std::convertible_to<bool>;
        {
            Ops::activate_binding(binding)
        } noexcept;
        {
            Ops::flush_tlb()
        } noexcept;
    };
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
