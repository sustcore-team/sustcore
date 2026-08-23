/**
 * @file mem_seg.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 匿名、固定大小且按需分配物理页的 MemSeg 对象。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <error/mem_seg.h>
#include <memory/physical/gfp.h>
#include <obj/kobject.h>
#include <sustcore/addr.h>
#include <synchronized.h>
#include <tay/expected.h>
#include <tay/map.h>

#include <cstddef>
#include <utility>

namespace memory {
    class MemSeg final : public cap::TypedKObject<MemSeg, cap::ObjectType::MEMORY> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::MEMORY;

        [[nodiscard]] static tay::expected<cap::KObjectRef<MemSeg>, MemSegError> create(
            size_t bytes) noexcept;

        MemSeg(const MemSeg &)            = delete;
        MemSeg &operator=(const MemSeg &) = delete;
        MemSeg(MemSeg &&)                 = delete;
        MemSeg &operator=(MemSeg &&)      = delete;
        ~MemSeg() noexcept;

        [[nodiscard]] size_t size() const noexcept {
            return size_;
        }

        /** @brief 返回页索引的锁内快照，不能在并发 ensure_page() 时直接读取 hash map。 */
        [[nodiscard]] size_t allocated_size() const noexcept;

        [[nodiscard]] tay::expected<PhyAddr, MemSegError> ensure_page(size_t offset) noexcept;
        [[nodiscard]] tay::expected<PhyAddr, MemSegError> lookup_page(size_t offset) const noexcept;
        [[nodiscard]] tay::expected<size_t, MemSegError> write(size_t offset, const void *data,
                                                               size_t buflen) noexcept;

    private:
        struct State final {
            explicit State(tay::hash_map<size_t, OwnedPages> &&initial_pages) noexcept
                : pages(std::move(initial_pages)) {}

            tay::hash_map<size_t, OwnedPages> pages;
        };

        explicit MemSeg(size_t bytes, tay::hash_map<size_t, OwnedPages> &&pages) noexcept
            : size_(bytes), state_(std::move(pages)) {}

        size_t size_ = 0;
        kernel::simple_synchronized<State> state_;
    };
}  // namespace memory
