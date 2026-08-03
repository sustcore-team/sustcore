/**
 * @file kernel_layout.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核映像各段的最终 W^X 映射
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <memory/virtual/kernel/kernel_space_internal.h>
#include <memory/virtual/kernel/symbols.h>
#include <sustcore/addrspace.h>

namespace memory::detail {
    PhyAddr kernel_symbol_paddr(const char *symbol) noexcept {
        const auto address = reinterpret_cast<addr_t>(symbol);
        if (address < KVA_START)
            kernel::log::panic("内核符号位于高半区映像之外");
        return PhyAddr(address - KVA_START);
    }

    PhyAddr kernel_start_paddr() noexcept {
        return kernel_symbol_paddr(skernel);
    }
    PhyAddr kernel_end_paddr() noexcept {
        return kernel_symbol_paddr(ekernel);
    }

    tay::expected<void, tay::error_code> map_kernel_layout(KernelSpace &space) noexcept {
        const auto map_segment = [&space](char *begin, char *end, PageFlags flags) {
            if (begin == end)
                return tay::expected<void, tay::error_code>{};
            return space.map(reinterpret_cast<addr_t>(begin), kernel_symbol_paddr(begin),
                             static_cast<size_t>(end - begin), flags);
        };

        auto result = map_segment(
            s_text, e_text, PageFlags{.readable = true, .writable = false, .executable = true});
        if (!result)
            return result;
        result = map_segment(s_rodata, e_rodata,
                             PageFlags{.readable = true, .writable = false, .executable = false});
        if (!result)
            return result;
        result = map_segment(s_data, e_data,
                             PageFlags{.readable = true, .writable = true, .executable = false});
        if (!result)
            return result;
        result = map_segment(__bsp_stack_bottom, __bsp_stack_top,
                             PageFlags{.readable = true, .writable = true, .executable = false});
        if (!result)
            return result;
        result = map_segment(s_bss, e_bss,
                             PageFlags{.readable = true, .writable = true, .executable = false});
        if (!result)
            return result;
        result = map_segment(s_init_text, e_init_text,
                             PageFlags{.readable = true, .writable = false, .executable = true});
        if (!result)
            return result;
        result = map_segment(s_init_rodata, e_init_rodata,
                             PageFlags{.readable = true, .writable = false, .executable = false});
        if (!result)
            return result;
        result = map_segment(s_init_data, e_init_data,
                             PageFlags{.readable = true, .writable = true, .executable = false});
        if (!result)
            return result;
        return map_segment(s_init_bss, e_init_bss,
                           PageFlags{.readable = true, .writable = true, .executable = false});
    }
}  // namespace memory::detail
