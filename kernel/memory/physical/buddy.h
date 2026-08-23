/**
 * @file buddy.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核物理页分配器接口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addr.h>
#include <synchronized.h>
#include <tay/bits.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <cstddef>

namespace memory {
    /** @brief 描述一段由 Buddy 分配、以页为单位连续的物理内存。 */
    struct PageAlloc {
        PhyAddr base{};
        size_t pages = 0;

        /** @brief 判断分配是否同时具有非空基址和非零页数。 */
        [[nodiscard]] explicit operator bool() const noexcept {
            return base.nonnull() && pages != 0;
        }
    };

    /**
     * @brief 管理全局物理页的 Buddy 分配器。
     *
     * 除初始化早期外，调用者必须通过 buddy() 获取实例，以便由同一把 preempt-safe
     * ticket spinlock 串行化所有 free-list 和 descriptor pool 操作。硬中断不得访问 Buddy。
     */
    class Buddy final {
    public:
        static constexpr size_t MAX_ORDER       = 30;
        static constexpr size_t DESC_POOL_PAGES = 4;

        /** @brief 常量构造一个尚未挂接 descriptor pool 的空 Buddy。 */
        constexpr Buddy() noexcept
            : permanent_pool_{},
              free_{},
              pool_head_(nullptr),
              pool_tail_(nullptr),
              runtime_pool_count_(0),
              free_pages_(0),
              initialized_(false),
              expanding_pool_(false) {}

        /**
         * @brief 初始化并挂接 Buddy 永久持有的 descriptor pool。
         * @note 永久 pool 嵌入 Buddy 的正常 BSS 对象中，不依赖可回收 init 段或物理分配。
         */
        void initialize() noexcept;

        /**
         * @brief 将页对齐的物理区域加入可分配集合。
         * @param area 待释放给 Buddy 的物理地址范围。
         * @note 调用者必须持有 buddy() 返回的全局锁定引用。
         */
        void add_range(PhyArea area) noexcept;

        /**
         * @brief 分配 `2^order` 个连续且自然对齐的物理页。
         * @param order 以二为底的页数阶数，最大为 MAX_ORDER。
         * @return 成功时返回连续页范围；资源不足或参数越界时返回错误。
         */
        [[nodiscard]] tay::expected<PageAlloc, tay::error_code> try_alloc_order(
            size_t order) noexcept;

        /**
         * @brief 分配至少指定页数并满足页粒度对齐的连续物理内存。
         * @param pages 请求页数。
         * @param alignment_pages 以页为单位的二次幂对齐，默认单页对齐。
         * @return 成功时返回实际分配范围；参数无效或无可用块时返回错误。
         */
        [[nodiscard]] tay::expected<PageAlloc, tay::error_code> try_alloc_pages(
            size_t pages, size_t alignment_pages = 1) noexcept;

        /**
         * @brief 将完整 PageAlloc 归还物理页分配器。
         * @param allocation 先前由同一 Buddy 返回的分配；空分配会被忽略。
         * @warning 重复释放、范围越界或释放非 Buddy 分配会触发 panic。
         */
        void free_pages(PageAlloc allocation) noexcept;

        /** @brief 返回当前可分配物理页数的锁内快照。 */
        [[nodiscard]] size_t free_pages() const noexcept {
            return free_pages_;
        }

        /** @brief 返回永久 pool 与运行期 pool 合计占用的元数据页数。 */
        [[nodiscard]] size_t metadata_pages() const noexcept {
            return (runtime_pool_count_ + 1) * DESC_POOL_PAGES;
        }

#ifndef NDEBUG
        [[nodiscard]] size_t debug_pool_count() const noexcept {
            return runtime_pool_count_;
        }
#endif

        /** @brief 判断永久 descriptor pool 是否已经初始化并挂接。 */
        [[nodiscard]] bool initialized() const noexcept {
            return initialized_;
        }

    private:
        struct DescPool;

        struct FreeBlock {
            FreeBlock *previous = nullptr;
            FreeBlock *next     = nullptr;
            DescPool *pool      = nullptr;
            addr_t physical     = 0;
            u32_t order         = 0;
            u16_t slot          = 0;
            u16_t magic         = 0;
        };

        static constexpr size_t DESCS_PER_POOL     = 384;
        static constexpr size_t DESC_BITMAP_WORDS  = DESCS_PER_POOL / 64;
        static constexpr size_t DESCRIPTOR_RESERVE = MAX_ORDER + 2;
        static constexpr u16_t DESCRIPTOR_MAGIC    = 0xBADD;

        struct DescPool {
            DescPool *previous = nullptr;
            DescPool *next     = nullptr;
            PageAlloc backing{};
            size_t used = 0;
            u64_t bitmap[DESC_BITMAP_WORDS]{};
            FreeBlock blocks[DESCS_PER_POOL]{};
        };

        static_assert(sizeof(DescPool) <= DESC_POOL_PAGES * PAGE_SIZE);

        alignas(64) DescPool permanent_pool_;
        FreeBlock *free_[MAX_ORDER + 1];
        DescPool *pool_head_;
        DescPool *pool_tail_;
        size_t runtime_pool_count_;
        size_t free_pages_;
        bool initialized_;
        bool expanding_pool_;

        static size_t ceil_order(size_t pages) noexcept;
        static void init_pool(DescPool &pool, PageAlloc backing) noexcept;
        void attach_pool(DescPool &pool) noexcept;
        [[nodiscard]] size_t free_descs() const noexcept;
        [[nodiscard]] FreeBlock *allocate_from_pool(DescPool &pool) noexcept;
        [[nodiscard]] FreeBlock *alloc_desc() noexcept;
        void free_desc(FreeBlock &block) noexcept;
        void ensure_descs() noexcept;
        void add_runtime_pool() noexcept;

        FreeBlock *find(size_t order, addr_t physical) noexcept;
        FreeBlock *insert(addr_t physical, size_t order) noexcept;
        void remove(FreeBlock &block) noexcept;
        void release_block(addr_t physical, size_t order) noexcept;
        void release_range(addr_t physical, size_t pages) noexcept;
        tay::expected<addr_t, tay::error_code> allocate_block(size_t order) noexcept;
    };

    /**
     * @brief 获取全局 Buddy 对象及其 preempt-safe 独占锁。
     * @return 生命周期内持有本地抢占保护与全局 Buddy ticket spinlock 的引用。
     * @note `Buddy::initialize()` 可在 MEMORY_READY 前通过此入口调用；
     * 其余操作要求 `buddy_ready()`。
     */
    [[nodiscard]] kernel::locked_ref<Buddy> buddy() noexcept;

    /** @brief 查询全局 Buddy 是否已经挂接永久 descriptor pool。 */
    [[nodiscard]]
    static __ATTR_ALWAYS_INLINE__ bool buddy_ready() noexcept {
        return buddy()->initialized();
    }
}  // namespace memory
