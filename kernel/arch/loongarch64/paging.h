/**
 * @file paging.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LoongArch64 四级页表策略声明
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/paging.h>

namespace loongarch64::hal {
    struct PtOps final {
        using EntryType                           = u64_t;
        static constexpr size_t ENTRIES_PER_TABLE = PAGE_SIZE / sizeof(EntryType);
        static constexpr size_t TOP_LEVEL         = 3;
        static constexpr bool SHARES_HIGH_ROOT    = false;

        [[nodiscard]] static constexpr size_t index_at(addr_t address, size_t level) noexcept {
            return (address >> (12 + level * 9)) & 0x1ff;
        }
        [[nodiscard]] static EntryType *table(PhyAddr physical) noexcept;
        [[nodiscard]] static EntryType load_entry(const EntryType *entry) noexcept;
        static void store_leaf(EntryType *entry, EntryType value) noexcept;
        static void publish_table(EntryType *entry, EntryType value) noexcept;
        [[nodiscard]] static bool present(EntryType entry) noexcept;
        [[nodiscard]] static bool leaf(EntryType entry) noexcept;
        [[nodiscard]] static PhyAddr next_table(EntryType entry) noexcept;
        [[nodiscard]] static EntryType make_table(PhyAddr physical) noexcept;
        [[nodiscard]] static tay::expected<EntryType, tay::error_code> make_leaf(
            PhyAddr physical, const memory::PageFlags &flags) noexcept;
        [[nodiscard]] static memory::PageFlags decode_flags(EntryType entry) noexcept;
        [[nodiscard]] static PhyAddr leaf_physical(EntryType entry, addr_t address,
                                                   size_t level) noexcept;
        [[nodiscard]] static bool canonical(addr_t address) noexcept;
        static void activate_binding(const memory::RootBinding &binding) noexcept;
        static void flush_tlb() noexcept;
        [[nodiscard]] static u64_t debug_flushes() noexcept;
    };
}  // namespace loongarch64::hal
