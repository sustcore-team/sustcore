/**
 * @file pt.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 显式页表根的映射事务与结构生命周期
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/paging_traits.h>
#include <memory/virtual/flags.h>
#include <memory/virtual/pt_pool.h>
#include <memory/virtual/pt_walk.h>
#include <error/paging.h>
#include <tay/expected.h>

#include <cstddef>

namespace memory {
    class KernelVm;
    class UserVm;

    enum class PtKind : u8_t {
        KERNEL,
        USER,
    };

    class PageTable final {
    public:
        /** @brief 显式分配并声明一页新的空页表根。 */
        [[nodiscard]] static tay::expected<PhyAddr, PagingError> create_root(
            PtOwnerId owner) noexcept;

        /** @brief 释放尚未交给 PageTable 对象的空页表根。 */
        static void destroy_root(PhyAddr root, PtOwnerId owner) noexcept;

        /** @brief 构造尚未绑定根节点的空 PageTable，供静态单例常量初始化。 */
        constexpr PageTable() noexcept = default;

        /** @brief 取得已存在页表根及其私有中间页表的结构所有权。 */
        explicit PageTable(PhyAddr root, PtKind kind, PtOwnerId owner, u16_t asid = 0) noexcept;
        PageTable(const PageTable &)            = delete;
        PageTable &operator=(const PageTable &) = delete;
        PageTable(PageTable &&)                 = delete;
        PageTable &operator=(PageTable &&)      = delete;
        ~PageTable() noexcept;

        [[nodiscard]] PhyAddr root() const noexcept {
            return root_;
        }
        [[nodiscard]] PtKind kind() const noexcept {
            return kind_;
        }
        [[nodiscard]] PtOwnerId owner() const noexcept {
            return owner_;
        }
        using EntryType = hal::PtOps::EntryType;

    private:
        friend class KernelVm;
        friend class UserVm;

        [[nodiscard]] tay::expected<void, PagingError> adopt_root(PhyAddr root, PtKind kind,
                                                                  PtOwnerId owner,
                                                                  u16_t asid = 0) noexcept;

        [[nodiscard]] tay::expected<void, PagingError> map(addr_t vaddr, PhyAddr physical,
                                                           size_t bytes, PageFlags flags,
                                                           paging::WalkDomain domain) noexcept;
        [[nodiscard]] tay::expected<void, PagingError> unmap(addr_t vaddr, size_t bytes,
                                                             paging::WalkDomain domain) noexcept;
        [[nodiscard]] tay::expected<void, PagingError> protect(addr_t vaddr, size_t bytes,
                                                               PageFlags flags,
                                                               paging::WalkDomain domain) noexcept;
        [[nodiscard]] tay::expected<PageMapping, PagingError> query(
            addr_t vaddr, paging::WalkDomain domain) const noexcept;

        [[nodiscard]] EntryType root_entry(size_t index) const noexcept;
        [[nodiscard]] bool try_install_root(size_t index, EntryType value) noexcept;

        [[nodiscard]] static tay::expected<size_t, PagingError> checked_range(
            addr_t vaddr, size_t bytes, PagingError::Operation operation) noexcept;
        [[nodiscard]] static bool valid_flags(const PageFlags &flags) noexcept;
        void shootdown(addr_t vaddr, size_t pages) noexcept;

        PhyAddr root_{};
        PtKind kind_     = PtKind::KERNEL;
        PtOwnerId owner_ = 0;
        u16_t asid_      = 0;
    };
}  // namespace memory
