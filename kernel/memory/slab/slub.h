/**
 * @file slub.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核固定大小对象缓存模型与接口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <memory/physical/buddy.h>
#include <synchronized.h>
#include <tay/bits.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/list.h>

#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace memory {
    class MixedSlabsAllocator;

    inline constexpr size_t SLUB_CHUNK_SZ     = 256 * 1024;
    inline constexpr size_t SLUB_MAX_SMALL_SZ = 32 * 1024;

    /** @brief SLUB 分配器当前驻留与使用情况的瞬时统计。 */
    struct SlubStats {
        size_t chunks         = 0;
        size_t objects_in_use = 0;
        size_t objects_total  = 0;
        size_t resident_pages = 0;
    };

    namespace detail {
        class SlubCore;
        struct SlubChunk;
        template <size_t... Sizes>
        class SlubList;

        enum class AllocationKind : u32_t {
            SLAB  = 1,
            LARGE = 2,
        };

        inline constexpr u64_t ALLOCATION_MAGIC  = 0x534C5542414C4C4FULL;
        inline constexpr u64_t OVERALIGNED_MAGIC = 0x534C5542414C4947ULL;

        struct AllocationHeader {
            u64_t magic         = ALLOCATION_MAGIC;
            AllocationKind kind = AllocationKind::SLAB;
            u32_t reserved      = 0;
            SlubCore *owner     = nullptr;
            PageAllocation extent{};
            size_t slot_sz  = 0;
            size_t capacity = 0;
        };

        struct OveralignedPrefix {
            u64_t magic              = OVERALIGNED_MAGIC;
            AllocationHeader *header = nullptr;
        };

        enum class ChunkState : u8_t {
            EMPTY,
            PARTIAL,
            FULL,
        };

#ifndef NDEBUG
        constexpr size_t ALLOCATION_BITMAP_WORDS = SLUB_CHUNK_SZ / 16 / 64;
#endif

        struct SlubChunk {
            AllocationHeader allocation{};
            using chunklist_hook = tay::intrusive_list_hook<SlubChunk *, SlubChunk *>;
            chunklist_hook list_hook{};
            ChunkState state = ChunkState::EMPTY;
            void *local_free = nullptr;
            std::atomic<void *> remote_free{nullptr};
            size_t allocated_cnt = 0;
            size_t total_cnt     = 0;
            addr_t first_object  = 0;
#ifndef NDEBUG
            std::atomic<u64_t> allocation_bits[ALLOCATION_BITMAP_WORDS]{};
#endif
        };

        static_assert(offsetof(SlubChunk, allocation) == 0);
        static_assert(sizeof(SlubChunk) < SLUB_CHUNK_SZ);

        using chunk_list =
            tay::intrusive_list<SlubChunk, tay::locate_member<SlubChunk, SlubChunk::chunklist_hook,
                                                              &SlubChunk::list_hook>>;

        [[nodiscard]] AllocationHeader *header_for(void *ptr) noexcept;
        [[nodiscard]] tay::expected<void *, tay::error_code> allocate_large(
            size_t sz, size_t alignment, SlubCore *owner = nullptr) noexcept;
        void release_large(AllocationHeader &header, void *ptr) noexcept;

        class SlubCore final {
        public:
            constexpr SlubCore(size_t slot_sz, size_t slot_align) noexcept
                : slot_sz_(slot_sz), slot_align_(slot_align) {}

            SlubCore(const SlubCore &)            = delete;
            SlubCore &operator=(const SlubCore &) = delete;
            SlubCore(SlubCore &&)                 = delete;
            SlubCore &operator=(SlubCore &&)      = delete;

            [[nodiscard]] tay::expected<void *, tay::error_code> try_allocate() noexcept;
            void deallocate(void *ptr) noexcept;
            void trim() noexcept;

            [[nodiscard]] SlubStats stats() const noexcept;
            [[nodiscard]] constexpr size_t slot_sz() const noexcept {
                return slot_sz_;
            }

        private:
            class MutableState {
            public:
                [[nodiscard]] SlubChunk *select_chunk() noexcept;
                void adopt_chunk(SlubChunk &chunk) noexcept;
                [[nodiscard]] void *allocate_from(SlubChunk &chunk) noexcept;
                void deallocate_to(SlubChunk &chunk, void *ptr) noexcept;

                [[nodiscard]] PageAllocation detach_excess_empty(SlubChunk &chunk) noexcept;
                [[nodiscard]] PageAllocation detach_empty() noexcept;
                void drain_remote_frees() noexcept;

                void account_large_allocation(size_t pages) noexcept;
                void account_large_release(size_t pages) noexcept;
                [[nodiscard]] SlubStats stats(bool large_path) const noexcept;

            private:
                chunk_list partial_{};
                chunk_list full_{};
                chunk_list empty_{};
                size_t chunks_         = 0;
                size_t objects_in_use_ = 0;
                size_t objects_total_  = 0;
                size_t large_pages_    = 0;

                void move_to_partial(SlubChunk &chunk) noexcept;
                void move_to_full(SlubChunk &chunk) noexcept;
                void move_to_empty(SlubChunk &chunk) noexcept;
                void drain_remote(SlubChunk &chunk) noexcept;
                [[nodiscard]] PageAllocation detach_chunk(SlubChunk &chunk) noexcept;
            };

            size_t slot_sz_;
            size_t slot_align_;
            kernel::synchronized<MutableState> state_{};

            [[nodiscard]] bool uses_large_path() const noexcept {
                return slot_sz_ > SLUB_MAX_SMALL_SZ;
            }

            [[nodiscard]] SlubChunk *create_chunk() noexcept;
            void deallocate_remote(SlubChunk &chunk, void *ptr) noexcept;
        };

        [[nodiscard]] constexpr size_t align_up(size_t value, size_t alignment) noexcept {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        [[nodiscard]] constexpr size_t normalized_slot_sz(size_t sz) noexcept {
            const size_t minimum = sz < sizeof(void *) ? sizeof(void *) : sz;
            return align_up(minimum, alignof(std::max_align_t));
        }

        [[nodiscard]] constexpr size_t slot_align(size_t sz) noexcept {
            return sz & (~sz + 1);
        }

        [[nodiscard]] constexpr size_t typed_slot_sz(size_t sz, size_t alignment) noexcept {
            constexpr size_t CLASSES[] = {
                16,   32,   48,   64,   96,   128,  192,   256,   384,   512,   768,   1024,
                1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 20480, 24576, 32768,
            };
            for (auto candidate : CLASSES) {
                if (candidate >= sz && candidate % alignment == 0) {
                    return candidate;
                }
            }
            return align_up(sz, alignment);
        }
    }  // namespace detail

    /**
     * @brief 为固定请求大小提供无类型 SLUB 存储池。
     *
     * 小对象来自 256 KiB chunk；超出阈值的对象直接使用连续物理页。
     * 实例不可复制或移动，返回的对象可由其它 CPU 归还原 owner。
     *
     * @tparam N 调用者请求的最小对象字节数。
     */
    template <size_t N>
    class Slub final {
        friend class MixedSlabsAllocator;
        template <size_t... Sizes>
        friend class detail::SlubList;

    public:
        static_assert(N != 0, "Slub object size must be non-zero");

        static constexpr size_t REQUESTED_SZ = N;
        static constexpr size_t OBJECT_SZ    = detail::normalized_slot_sz(N);
        static constexpr size_t OBJECT_ALIGN = detail::slot_align(OBJECT_SZ);

        constexpr Slub() noexcept : core_(OBJECT_SZ, OBJECT_ALIGN) {}

        Slub(const Slub &)            = delete;
        Slub &operator=(const Slub &) = delete;
        Slub(Slub &&)                 = delete;
        Slub &operator=(Slub &&)      = delete;

        /**
         * @brief 分配一个未初始化对象槽位。
         * @return 成功时返回至少 OBJECT_SZ 字节的存储，否则返回分配错误。
         */
        [[nodiscard]] tay::expected<void *, tay::error_code> try_allocate() noexcept {
            return core_.try_allocate();
        }

        /**
         * @brief 释放先前由同一逻辑池分配的槽位。
         * @param ptr 待释放对象；nullptr 会被忽略。
         */
        void deallocate(void *ptr) noexcept {
            core_.deallocate(ptr);
        }

        /** @brief 返回单个槽位对调用者可用的固定字节数。 */
        [[nodiscard]] constexpr size_t usable_sz() const noexcept {
            return OBJECT_SZ;
        }

        /** @brief 返回在内部锁保护下采集的池统计快照。 */
        [[nodiscard]] SlubStats stats() const noexcept {
            return core_.stats();
        }

        /**
         * @brief 排空远程释放并把所有空闲 chunk 归还 Buddy。
         * @note 活跃对象及其地址保持不变。
         */
        void trim() noexcept {
            core_.trim();
        }

    private:
        detail::SlubCore core_;

        [[nodiscard]] detail::SlubCore &core() noexcept {
            return core_;
        }
    };

    /**
     * @brief 在固定 SLUB 上提供类型化对象存储和可选生命周期管理。
     * @tparam T 池中对象类型；构造和析构接口要求对应操作为 noexcept。
     */
    template <class T>
    class SlubPool final {
    public:
        static constexpr size_t SELECTED_SZ = detail::typed_slot_sz(sizeof(T), alignof(T));
        static constexpr size_t OBJECT_SZ   = detail::normalized_slot_sz(SELECTED_SZ);

        /** @brief 分配一块适合 T 的原始存储，不调用构造函数。 */
        [[nodiscard]] tay::expected<T *, tay::error_code> try_allocate() noexcept {
            return static_cast<T *>(TAY_TRY(storage_.try_allocate()));
        }

        /** @brief 释放原始存储，不调用 T 的析构函数。 */
        void deallocate(T *ptr) noexcept {
            storage_.deallocate(ptr);
        }

        /**
         * @brief 分配存储并以给定参数就地构造 T。
         * @return 成功时返回已构造对象；分配失败时返回错误。
         */
        template <class... Args>
            requires std::is_nothrow_constructible_v<T, Args &&...>
        [[nodiscard]] tay::expected<T *, tay::error_code> try_create(Args &&...args) noexcept {
            auto *storage = TAY_TRY(try_allocate());
            return new (storage) T(std::forward<Args>(args)...);
        }

        /**
         * @brief 调用 T 的析构函数并归还其存储。
         * @param ptr 由该池创建的对象；nullptr 会被忽略。
         */
        void destroy(T *ptr) noexcept
            requires std::is_nothrow_destructible_v<T>
        {
            if (ptr == nullptr)
                return;
            ptr->~T();
            deallocate(ptr);
        }

        /** @brief 返回底层固定大小池的统计快照。 */
        [[nodiscard]] SlubStats stats() const noexcept {
            return storage_.stats();
        }

        /** @brief 排空远程释放并释放底层池的空闲 chunk。 */
        void trim() noexcept {
            storage_.trim();
        }

    private:
        Slub<OBJECT_SZ> storage_{};
    };
}  // namespace memory
