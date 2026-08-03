/**
 * @file heap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核通用动态内存分配接口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <memory/slab/slub.h>
#include <tay/err.h>
#include <tay/expected.h>

#include <cstddef>

namespace memory {
    enum class HeapPhase : u8_t {
        OFFLINE,
        INITIALIZING,
        READY,
    };
    namespace detail {
        template <size_t... Sizes>
        class SlubList;

        template <>
        class SlubList<> final {
        public:
            static constexpr size_t COUNT = 0;

            [[nodiscard]] detail::SlubCore *find(size_t, size_t) noexcept {
                return nullptr;
            }

            [[nodiscard]] detail::SlubCore *find_exact(size_t) noexcept {
                return nullptr;
            }

            template <typename Visitor>
            constexpr void for_each(Visitor &) noexcept {}

            template <typename Visitor>
            constexpr void for_each(Visitor &) const noexcept {}
        };

        /** @brief 由编译期 size class 参数包生成的异构 SLUB 列表。 */
        template <size_t First, size_t... Rest>
        class SlubList<First, Rest...> final {
        public:
            static_assert(((First < Rest) && ...), "Slub size classes must be strictly ordered");

            static constexpr size_t COUNT                      = 1 + sizeof...(Rest);
            inline static constexpr size_t SIZE_CLASSES[COUNT] = {First, Rest...};

            constexpr SlubList() noexcept = default;

            SlubList(const SlubList &)            = delete;
            SlubList &operator=(const SlubList &) = delete;
            SlubList(SlubList &&)                 = delete;
            SlubList &operator=(SlubList &&)      = delete;

            [[nodiscard]] detail::SlubCore *find(size_t sz, size_t alignment) noexcept {
                if (alignment != 0 && Slub<First>::OBJECT_SZ >= sz &&
                    Slub<First>::OBJECT_SZ % alignment == 0)
                {
                    return &first_.core();
                }
                return rest_.find(sz, alignment);
            }

            [[nodiscard]] detail::SlubCore *find_exact(size_t sz) noexcept {
                if (Slub<First>::OBJECT_SZ == sz) {
                    return &first_.core();
                }
                return rest_.find_exact(sz);
            }

            template <typename Visitor>
            constexpr void for_each(Visitor &visitor) noexcept(noexcept(visitor(first_)) &&
                                                               noexcept(rest_.for_each(visitor))) {
                visitor(first_);
                rest_.for_each(visitor);
            }

            template <typename Visitor>
            constexpr void for_each(Visitor &visitor) const
                noexcept(noexcept(visitor(first_)) && noexcept(rest_.for_each(visitor))) {
                visitor(first_);
                rest_.for_each(visitor);
            }

        private:
            Slub<First> first_{};
            [[no_unique_address]] SlubList<Rest...> rest_{};
        };
    }  // namespace detail

    using HeapSlubList =
        detail::SlubList<16, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072,
                         4096, 6144, 8192, 12288, 16384, 20480, 24576, 32768>;

    inline constexpr size_t SLAB_CLASS_CNT                           = HeapSlubList::COUNT;
    inline constexpr const size_t (&SLAB_SZ_CLASSES)[SLAB_CLASS_CNT] = HeapSlubList::SIZE_CLASSES;

    /**
     * @brief 将常用 size class SLUB 与大对象页分配组合为内核堆 shard。
     *
     * 当前阶段由永久全局 shard 提供服务；对象头仍记录实际 owner，为未来 per-CPU
     * shard 保留跨 CPU 释放协议。
     * 各 size class 内部使用 IRQ-safe 锁，调用者不得在持有冲突锁时触发分配。
     */
    class MixedSlabsAllocator final {
    public:
        constexpr MixedSlabsAllocator() noexcept = default;

        MixedSlabsAllocator(const MixedSlabsAllocator &)            = delete;
        MixedSlabsAllocator &operator=(const MixedSlabsAllocator &) = delete;
        MixedSlabsAllocator(MixedSlabsAllocator &&)                 = delete;
        MixedSlabsAllocator &operator=(MixedSlabsAllocator &&)      = delete;

        /**
         * @brief 分配满足大小和对齐要求的未初始化存储。
         * @param sz 请求字节数；零会按最小非零分配处理。
         * @param alignment 二次幂字节对齐。
         * @return 成功时返回存储指针；堆未就绪、对齐无效或内存不足时返回错误。
         */
        [[nodiscard]] tay::expected<void *, tay::error_code> try_allocate(
            size_t sz, size_t alignment = alignof(std::max_align_t)) noexcept;
        /**
         * @brief 释放由任意 MixedSlabsAllocator shard 分配的存储。
         * @param ptr 待释放指针；nullptr 会被忽略。
         * @warning 非堆指针、重复释放或损坏的 allocation header 会触发 panic。
         */
        void deallocate(void *ptr) noexcept;

        /**
         * @brief 调整现有分配的可用容量。
         * @return 成功时返回原指针或替换指针；扩容失败返回 nullptr 且保留原分配。
         * @note `ptr == nullptr` 等价于分配；`new_sz == 0` 等价于释放。
         */
        [[nodiscard]] void *reallocate(void *ptr, size_t new_sz) noexcept;

        /**
         * @brief 查询分配对调用者可用的字节容量。
         * @return nullptr 返回 0；否则返回所属 size class 或大对象容量。
         */
        [[nodiscard]] size_t usable_sz(void *ptr) const noexcept;

        /**
         * @brief 排空远程释放链并归还该 shard 的全部空闲 SLUB chunk。
         * @note 活跃对象不移动；该操作可能取得多个 size class 锁并向 Buddy 归还物理页。
         */
        void trim() noexcept;

        /**
         * @brief 按 size class 升序访问该 shard 的所有 SLUB。
         * @param visitor 接受 `Slub<N> &` 的泛型可调用对象。
         */
        template <typename Visitor>
        constexpr void for_each_slub(Visitor &&visitor) noexcept(
            noexcept(slubs_.for_each(visitor))) {
            slubs_.for_each(visitor);
        }

        /** @copydoc for_each_slub */
        template <typename Visitor>
        constexpr void for_each_slub(Visitor &&visitor) const
            noexcept(noexcept(slubs_.for_each(visitor))) {
            slubs_.for_each(visitor);
        }

    private:
        HeapSlubList slubs_{};
    };

    using MixedSlabsAllocator = MixedSlabsAllocator;
    using SlabShard           = MixedSlabsAllocator;

    /**
     * @brief 探测并发布常量初始化的永久全局内核堆。
     * @note 仅允许 BSP 在 PageDatabase、Buddy 和启动 KPA 直映就绪后调用一次。
     */
    [[nodiscard]] tay::expected<void, tay::error_code> init_heap() noexcept;

    /** @brief 查询全局堆是否已用 release/acquire 语义发布。 */
    [[nodiscard]] bool heap_ready() noexcept;
    [[nodiscard]] MixedSlabsAllocator *try_heap_allocator() noexcept;

    /**
     * @brief 获取永久全局 MixedSlabsAllocator shard。
     * @warning 堆尚未发布时触发 panic。
     */
    MixedSlabsAllocator &heap_allocator() noexcept;
}  // namespace memory
