/**
 * @file gfp.h
 * @brief Buddy 与 PageDb 之间的原子页所有权入口。
 */

#pragma once

#include <memory/physical/buddy.h>
#include <memory/physical/page_db.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <cstddef>
#include <utility>

namespace memory {
    /**
     * @brief 同时拥有 Buddy 分配和 PageDb claim 的物理页范围。
     *
     * OwnedPages 是 move-only RAII 资源。析构会自动归还页面；`release()` 可用于在确定的
     * 时点显式归还。对象只保存释放所需的范围、角色和 owner，不隐藏额外分配。
     */
    class OwnedPages final {
    public:
        constexpr OwnedPages() noexcept           = default;
        OwnedPages(const OwnedPages &)            = delete;
        OwnedPages &operator=(const OwnedPages &) = delete;
        OwnedPages(OwnedPages &&other) noexcept;
        OwnedPages &operator=(OwnedPages &&other) noexcept;
        ~OwnedPages() noexcept;

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(allocation_);
        }

        [[nodiscard]] PhyAddr base() const noexcept {
            return allocation_.base;
        }

        [[nodiscard]] size_t pages() const noexcept {
            return allocation_.pages;
        }

        [[nodiscard]] size_t bytes() const noexcept {
            return allocation_.pages * PAGE_SIZE;
        }

        [[nodiscard]] PhyArea area() const noexcept {
            return PhyArea(allocation_.base, allocation_.base + bytes());
        }

        [[nodiscard]] PageKind kind() const noexcept {
            return kind_;
        }

        [[nodiscard]] u64_t owner_id() const noexcept {
            return owner_id_;
        }

        /** @brief 立即解除 PageDb 所有权并将完整范围归还 Buddy。 */
        void release() noexcept;

        /** @brief 恢复对已由 `detach()` 转移出去的页面范围的 RAII 所有权。 */
        [[nodiscard]] static OwnedPages resume(PageAlloc allocation, PageKind kind,
                                               u64_t owner_id) noexcept {
            return OwnedPages(std::move(allocation), kind, owner_id);
        }

        /**
         * @brief 将已 claim 页面的释放责任交给只保存 PageAlloc 的既有子系统。
         * @note 调用后本对象变为空；接收方必须最终通过 `resume()` 恢复 RAII 所有权。
         */
        [[nodiscard]] PageAlloc detach() noexcept;

    private:
        friend tay::expected<OwnedPages, tay::error_code> gfp(size_t, size_t, PageKind,
                                                              u64_t) noexcept;

        constexpr OwnedPages(PageAlloc allocation, PageKind kind, u64_t owner_id) noexcept
            : allocation_(std::move(allocation)), kind_(kind), owner_id_(owner_id) {}

        PageAlloc allocation_{};
        PageKind kind_  = PageKind::GENERIC;
        u64_t owner_id_ = 0;
    };

    /**
     * @brief 分配物理页并在同一 Buddy 临界区内完成 PageDb claim。
     */
    [[nodiscard]] tay::expected<OwnedPages, tay::error_code> gfp(size_t pages,
                                                                 size_t alignment_pages,
                                                                 PageKind kind,
                                                                 u64_t owner_id) noexcept;

    [[nodiscard]] inline tay::expected<OwnedPages, tay::error_code> gfp(size_t pages, PageKind kind,
                                                                        u64_t owner_id) noexcept {
        return gfp(pages, 1, kind, owner_id);
    }

}  // namespace memory
