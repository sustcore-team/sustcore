/**
 * @file page_table.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 显式页表根的映射事务与结构生命周期
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/paging_traits.h>
#include <memory/virtual/page_flags.h>
#include <memory/virtual/page_table_pool.h>
#include <memory/virtual/page_table_walker.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <cstddef>

namespace memory {
    class KernelSpace;
    class ClientSpace;

    enum class PageTableKind : u8_t {
        KERNEL,
        USER,
    };

    class PageTable final {
    public:
        /** @brief 显式分配并声明一页新的空页表根。 */
        [[nodiscard]] static tay::expected<PhyAddr, tay::error_code> create_root(
            PageTableOwnerId owner) noexcept;

        /** @brief 释放尚未交给 PageTable 对象的空页表根。 */
        static void destroy_root(PhyAddr root, PageTableOwnerId owner) noexcept;

        /** @brief 构造尚未绑定根节点的空 PageTable，供静态单例常量初始化。 */
        constexpr PageTable() noexcept = default;

        /** @brief 取得已存在页表根及其私有中间页表的结构所有权。 */
        explicit PageTable(PhyAddr root, PageTableKind kind, PageTableOwnerId owner) noexcept;
        PageTable(const PageTable &)            = delete;
        PageTable &operator=(const PageTable &) = delete;
        PageTable(PageTable &&)                 = delete;
        PageTable &operator=(PageTable &&)      = delete;
        ~PageTable() noexcept;

        [[nodiscard]] PhyAddr root() const noexcept {
            return root_;
        }
        [[nodiscard]] PageTableKind kind() const noexcept {
            return kind_;
        }
        [[nodiscard]] PageTableOwnerId owner() const noexcept {
            return owner_;
        }
        using EntryType = hal::PageTableOps::EntryType;

    private:
        friend class KernelSpace;
        friend class ClientSpace;

        [[nodiscard]] tay::expected<void, tay::error_code> adopt_root(
            PhyAddr root, PageTableKind kind, PageTableOwnerId owner) noexcept;

        [[nodiscard]] tay::expected<void, tay::error_code> map(addr_t vaddr, PhyAddr physical,
                                                               size_t bytes, PageFlags flags,
                                                               paging::WalkDomain domain) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> unmap(
            addr_t vaddr, size_t bytes, paging::WalkDomain domain) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> protect(
            addr_t vaddr, size_t bytes, PageFlags flags, paging::WalkDomain domain) noexcept;
        [[nodiscard]] tay::expected<PageMapping, tay::error_code> query(
            addr_t vaddr, paging::WalkDomain domain) const noexcept;

        [[nodiscard]] EntryType root_entry(size_t index) const noexcept;
        [[nodiscard]] bool install_root_entry_if_empty(size_t index, EntryType value) noexcept;

        [[nodiscard]] static tay::expected<size_t, tay::error_code> checked_range(
            addr_t vaddr, size_t bytes) noexcept;
        [[nodiscard]] static bool valid_flags(const PageFlags &flags) noexcept;

        PhyAddr root_{};
        PageTableKind kind_     = PageTableKind::KERNEL;
        PageTableOwnerId owner_ = 0;
    };
}  // namespace memory
