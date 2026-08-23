/**
 * @file bootinfo.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 通用 BootInfo 构造框架
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <boot/boot.h>
#include <boot/common/region.h>
#include <libfdt.h>
#include <tay/bits.h>

#include <cstddef>
#include <cstring>

namespace boot {
    enum class BootInfoBuildError : u8_t {
        INVALID_DTB,
        INVALID_REGION,
        REGION_CAPACITY,
        OUTPUT_TOO_LARGE,
        OUTPUT_CAPACITY,
    };

    template <size_t CAPACITY, auto Panic>
    class BootInfoBuilder final {
    public:
        constexpr BootInfoBuilder() noexcept = default;

        __ATTR_ALWAYS_INLINE__ explicit BootInfoBuilder(const void *dtb,
                                                        int default_size_cells) noexcept
            : dtb_(dtb), default_size_cells_(default_size_cells) {
            validate_input();
        }

        __ATTR_ALWAYS_INLINE__ void reset(const void *dtb, int default_size_cells) noexcept {
            dtb_                = dtb;
            default_size_cells_ = default_size_cells;
            memory_cnt_         = 0;
            reserved_cnt_       = 0;
            final_cnt_          = 0;
            validate_input();
        }

        [[nodiscard, gnu::always_inline]] size_t dtb_sz() const noexcept {
            return static_cast<size_t>(fdt_totalsize(dtb_));
        }

        __ATTR_ALWAYS_INLINE__ void collect_fdt_areas() noexcept {
            int node = -1;
            while ((node = fdt_next_node(dtb_, node, nullptr)) >= 0) {
                if (!node_enabled(node))
                    continue;
                int len = 0;
                const auto *device_type =
                    static_cast<const char *>(fdt_getprop(dtb_, node, "device_type", &len));
                if (device_type != nullptr && strcmp(device_type, "memory") == 0)
                    append_reg_regions(node, memory_, memory_cnt_, MemoryType::FREE);
            }

            const int reserved = fdt_path_offset(dtb_, "/reserved-memory");
            if (reserved < 0)
                return;
            int child = 0;
            fdt_for_each_subnode(child, dtb_, reserved) {
                if (node_enabled(child))
                    append_reg_regions(child, reserved_, reserved_cnt_, MemoryType::RESERVED);
            }
        }

        __ATTR_ALWAYS_INLINE__ void append_memory(PhyArea area,
                                                  MemoryType type = MemoryType::FREE) noexcept {
            append(memory_, memory_cnt_, area, type);
        }

        __ATTR_ALWAYS_INLINE__ void append_reserved(PhyArea area, MemoryType type) noexcept {
            append(reserved_, reserved_cnt_, area, type);
        }

        [[nodiscard, gnu::always_inline]] BootInfoHeader *write(addr_t cursor, addr_t limit,
                                                                size_t alignment,
                                                                PhyAddr dtb_paddr) noexcept {
            // 先归一化父区域与 reservation chain，最终区域数决定尾随对象的精确字节数。
            const auto final_cnt = normalize_regions();
            if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
                cursor > addr_t(-1) - (alignment - 1))
                fail(BootInfoBuildError::OUTPUT_CAPACITY);
            const addr_t aligned_cursor = (cursor + alignment - 1) & ~(alignment - 1);
            if (final_cnt > (MAX_BOOTINFO_SIZE - sizeof(BootInfoHeader) - sizeof(PhyAddr)) /
                                sizeof(MemoryRegion))
                fail(BootInfoBuildError::OUTPUT_TOO_LARGE);
            const size_t info_sz =
                sizeof(BootInfoHeader) + sizeof(MemoryRegion) * final_cnt + sizeof(PhyAddr);
            if (info_sz > MAX_BOOTINFO_SIZE)
                fail(BootInfoBuildError::OUTPUT_TOO_LARGE);
            if (aligned_cursor > limit || info_sz > limit - aligned_cursor)
                fail(BootInfoBuildError::OUTPUT_CAPACITY);

            // 调用者提供的原始内存承载 header、MemoryRegion 数组和 FDT 地址槽的连续布局。
            auto *header       = reinterpret_cast<BootInfoHeader *>(aligned_cursor);
            header->info_sz    = info_sz;
            header->region_cnt = final_cnt;
            auto *regions      = bootinfo_regions(header);
            for (size_t idx = 0; idx < final_cnt; ++idx) regions[idx] = final_[idx];
            *bootinfo_fdt_pa(header) = dtb_paddr;
            return header;
        }

    private:
        static constexpr size_t CELL_SZ = sizeof(fdt32_t);

        __ATTR_ALWAYS_INLINE__ void validate_input() const noexcept {
            if (dtb_ == nullptr || fdt_check_header(dtb_) != 0 ||
                (default_size_cells_ != 1 && default_size_cells_ != 2))
                fail(BootInfoBuildError::INVALID_DTB);
        }

        [[noreturn, gnu::always_inline]] static void fail(BootInfoBuildError error) noexcept {
            Panic(error);
            __builtin_unreachable();
        }

        __ATTR_ALWAYS_INLINE__ void append(MemoryRegion *regions, size_t &cnt, PhyArea area,
                                           MemoryType type) noexcept {
            if (area.nullable())
                return;
            if (area.end.arith() < area.begin.arith())
                fail(BootInfoBuildError::INVALID_REGION);
            if (cnt == CAPACITY)
                fail(BootInfoBuildError::REGION_CAPACITY);
            regions[cnt] = MemoryRegion{.area = area, .type = type, .rsvd_idx = cnt};
            ++cnt;
        }

        [[nodiscard, gnu::always_inline]] u64_t read_be(const void *data,
                                                        int cells) const noexcept {
            if (cells <= 0 || cells > 2)
                fail(BootInfoBuildError::INVALID_DTB);
            const auto *value = static_cast<const fdt32_t *>(data);
            u64_t result      = 0;
            // FDT 的多 cell 整数按网络序由高到低拼接，不能直接按宿主整数读取。
            for (int idx = 0; idx < cells; ++idx)
                result = (result << 32) | fdt32_to_cpu(value[idx]);
            return result;
        }

        [[nodiscard, gnu::always_inline]] int parent_cell_cnt(int node, const char *name,
                                                              int fallback) const noexcept {
            int parent = fdt_parent_offset(dtb_, node);
            if (parent < 0)
                parent = 0;
            int len          = 0;
            const auto *prop = static_cast<const fdt32_t *>(fdt_getprop(dtb_, parent, name, &len));
            if (prop == nullptr || len != static_cast<int>(sizeof(fdt32_t)))
                return fallback;
            const auto value = static_cast<int>(fdt32_to_cpu(*prop));
            if (value <= 0 || value > 2)
                fail(BootInfoBuildError::INVALID_DTB);
            return value;
        }

        [[nodiscard, gnu::always_inline]] bool node_enabled(int node) const noexcept {
            int len            = 0;
            const auto *status = static_cast<const char *>(fdt_getprop(dtb_, node, "status", &len));
            return status == nullptr || strcmp(status, "okay") == 0 || strcmp(status, "ok") == 0;
        }

        __ATTR_ALWAYS_INLINE__ void append_reg_regions(int node, MemoryRegion *regions, size_t &cnt,
                                                       MemoryType type) noexcept {
            int len                  = 0;
            const void *reg          = fdt_getprop(dtb_, node, "reg", &len);
            const int addr_cells     = parent_cell_cnt(node, "#address-cells", 2);
            const int size_cells     = parent_cell_cnt(node, "#size-cells", default_size_cells_);
            const int cells_per_item = addr_cells + size_cells;
            if (reg == nullptr || len <= 0 || len % static_cast<int>(cells_per_item * CELL_SZ) != 0)
                return;

            const auto *ptr  = static_cast<const std::byte *>(reg);
            const int stride = cells_per_item * static_cast<int>(CELL_SZ);
            for (int off = 0; off < len; off += stride) {
                const auto begin = static_cast<addr_t>(read_be(ptr + off, addr_cells));
                const auto sz =
                    static_cast<addr_t>(read_be(ptr + off + addr_cells * CELL_SZ, size_cells));
                if (sz > addr_t(-1) - begin)
                    fail(BootInfoBuildError::INVALID_REGION);
                append(regions, cnt, PhyArea(PhyAddr(begin), PhyAddr(begin + sz)), type);
            }
        }

        [[nodiscard, gnu::always_inline]] size_t normalize_regions() noexcept {
            // FREE 区域保留为父区域；reservation 作为其重叠子区域追加到父区域前缀之后。
            memory_cnt_   = region::normalize(memory_, memory_cnt_);
            reserved_cnt_ = region::normalize(reserved_, reserved_cnt_);
            final_cnt_    = 0;
            for (size_t idx = 0; idx < memory_cnt_; ++idx) {
                append(final_, final_cnt_, memory_[idx].area, MemoryType::FREE);
            }

            const size_t parent_cnt = final_cnt_;
            for (size_t parent_idx = 0; parent_idx < parent_cnt; ++parent_idx) {
                size_t previous_child = parent_idx;
                for (size_t child_idx = 0; child_idx < reserved_cnt_; ++child_idx) {
                    const addr_t begin = reserved_[child_idx].area.begin.arith() >
                                                 final_[parent_idx].area.begin.arith()
                                             ? reserved_[child_idx].area.begin.arith()
                                             : final_[parent_idx].area.begin.arith();
                    const addr_t end =
                        reserved_[child_idx].area.end.arith() < final_[parent_idx].area.end.arith()
                            ? reserved_[child_idx].area.end.arith()
                            : final_[parent_idx].area.end.arith();
                    if (begin >= end)
                        continue;
                    append(final_, final_cnt_, PhyArea(PhyAddr(begin), PhyAddr(end)),
                           reserved_[child_idx].type);
                    const size_t appended = final_cnt_ - 1;
                    if (previous_child == parent_idx)
                        final_[parent_idx].rsvd_idx = appended;
                    else
                        final_[previous_child].rsvd_idx = appended;
                    previous_child = appended;
                }
            }

            for (size_t idx = 0; idx < final_cnt_; ++idx) {
                if (!final_[idx].area.begin.template aligned<PAGE_SIZE>() ||
                    !final_[idx].area.end.template aligned<PAGE_SIZE>())
                    fail(BootInfoBuildError::INVALID_REGION);
            }
            return final_cnt_;
        }

        const void *dtb_        = nullptr;
        int default_size_cells_ = 1;
        MemoryRegion memory_[CAPACITY]{};
        MemoryRegion reserved_[CAPACITY]{};
        MemoryRegion final_[CAPACITY]{};
        size_t memory_cnt_   = 0;
        size_t reserved_cnt_ = 0;
        size_t final_cnt_    = 0;
    };
}  // namespace boot
