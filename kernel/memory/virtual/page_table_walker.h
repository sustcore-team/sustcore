/**
 * @file page_table_walker.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 当前架构页表的单页遍历与变更原语
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <arch/paging_traits.h>
#include <memory/virtual/page_flags.h>
#include <memory/virtual/page_table_pool.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <cstddef>

namespace memory::paging {
    enum class WalkDomain : u8_t {
        USER_PRIVATE,
        KERNEL_OWNED,
        BORROWED_KERNEL_READ_ONLY,
    };

    /** @brief walker 返回的映射及其叶项粒度。 */
    struct Mapping final {
        PageMapping mapping{};
        size_t level     = 0;
        size_t page_size = PAGE_SIZE;
    };

    /**
     * @brief 不拥有根的当前架构页表 walker。
     *
     * 调用者必须在所有写操作期间持有对应 PageTable 的锁。范围事务、leaf frame
     * map_count、TLB shootdown 和表页最终退役仍属于 PageTable。
     */
    class Walker final {
    public:
        Walker(PhyAddr root, PageTableOwnerId owner, WalkDomain domain) noexcept
            : root_(root), owner_(owner), domain_(domain) {}
        Walker(const Walker &)            = delete;
        Walker &operator=(const Walker &) = delete;

        /** @brief 查找地址所在的叶项；只读且绝不分配。 */
        [[nodiscard]] tay::expected<Mapping, tay::error_code> query(addr_t address) const noexcept;

        /**
         * @brief 在未映射的 base-page 位置安装叶项。
         *
         * 只在必要时建立中间页表。任何失败都会撤销本次新建立的父链，并将相应表页交给
         * retirement sink；调用者在 TLB 失效之后再归还它们。
         */
        [[nodiscard]] tay::expected<void, tay::error_code> try_map_base(
            addr_t address, PhyAddr physical, const PageFlags &flags,
            RetirementSink &retirements) noexcept;

        /** @brief 修改既有 base-page 叶项的属性，返回修改前的映射。 */
        [[nodiscard]] tay::expected<Mapping, tay::error_code> protect_base(
            addr_t address, const PageFlags &flags) noexcept;

        /**
         * @brief 清除既有 base-page 叶项，返回旧映射。
         *
         * 随后会摘下变空的中间表，并把它们交给 retirement sink；此函数不直接 retire。
         */
        [[nodiscard]] tay::expected<Mapping, tay::error_code> unmap_base(
            addr_t address, RetirementSink &retirements) noexcept;

        /**
         * @brief 替换既有 base-page 叶项，返回替换前映射，不创建中间表。
         *
         * 当前公开 PageTable::map() 不使用此接口；它保留给将来的 COW/换页路径。
         */
        [[nodiscard]] tay::expected<Mapping, tay::error_code> replace_base(
            addr_t address, PhyAddr physical, const PageFlags &flags) noexcept;

    private:
        using EntryType = hal::PageTableOps::EntryType;

        [[nodiscard]] static bool valid_flags(const PageFlags &flags) noexcept;
        [[nodiscard]] static bool valid_base_address(addr_t address) noexcept;
        [[nodiscard]] static Mapping mapping_for(EntryType entry, addr_t address,
                                                 size_t level) noexcept;
        [[nodiscard]] static bool table_empty(PhyAddr physical) noexcept;
        [[nodiscard]] tay::expected<EntryType, tay::error_code> base_leaf(
            addr_t address) const noexcept;
        [[nodiscard]] EntryType *leaf_entry(addr_t address) const noexcept;

        PhyAddr root_{};
        PageTableOwnerId owner_ = 0;
        WalkDomain domain_      = WalkDomain::KERNEL_OWNED;
    };

    using LeafVisitor = void (*)(const Mapping &) noexcept;

    /**
     * @brief 遍历并摘下整棵由当前 PageTable 拥有的页表树。
     *
     * 所有页表页（包括 root）均只加入 retirement sink；调用者必须在失效完成后实际 retire。
     */
    void detach_owned_tree(PhyAddr root, PageTableOwnerId owner, WalkDomain domain,
                           RetirementSink &retirements, LeafVisitor on_leaf) noexcept;
}  // namespace memory::paging
