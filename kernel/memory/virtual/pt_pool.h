/**
 * @file pt_pool.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Buddy 外部的页表页所有权适配
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <sustcore/addr.h>
#include <tay/bits.h>
#include <tay/err.h>
#include <tay/expected.h>

namespace memory {
    using PtOwnerId = u64_t;

    namespace paging {
        /**
         * @brief 页表页的具体分配/回收服务。
         *
         * 页表操作层在每次构建中只面对已选架构，不通过 allocator 模板参数泛化。
         */
        class PageAllocator final {
        public:
            [[nodiscard]] static tay::expected<PhyAddr, tay::error_code> allocate(
                PtOwnerId owner) noexcept;

            /**
             * @brief 归还已经完成 TLB 失效的页表页。
             *
             * 调用者必须保证任何 CPU 都不会再通过旧的页表遍历缓存访问该页。
             */
            static void retire(PhyAddr page, PtOwnerId owner) noexcept;
        };

        /**
         * @brief 范围事务使用的无分配页表页退役队列。
         *
         * 队列链接保存在 PageDesc 元数据中，而不触碰已经从页表树摘下的页表页内容；
         * 这样在 shootdown 完成前仍可安全保留硬件 page-table walker 所见的旧内容。
         */
        class RetirementSink final {
        public:
            explicit RetirementSink(PtOwnerId owner) noexcept : owner_(owner) {}
            RetirementSink(const RetirementSink &)            = delete;
            RetirementSink &operator=(const RetirementSink &) = delete;

            void defer_table(PhyAddr page) noexcept;
            void retire_all() noexcept;
            [[nodiscard]] bool empty() const noexcept {
                return !head_.nonnull();
            }

        private:
            PtOwnerId owner_ = 0;
            PhyAddr head_{};
        };
    }  // namespace paging
}  // namespace memory
