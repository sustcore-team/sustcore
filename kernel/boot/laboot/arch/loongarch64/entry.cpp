/**
 * @file entry.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LABOOT LoongArch 早期分页入口
 * @version 0.1.0-dev.1
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/loongarch64/valdef.h>
#include <boot/laboot/arch/loongarch64/early_paging.h>
#include <tay/bits.h>

#include <cstddef>

#define _LABOOT_STRING(x) _LABOOT_RODATA constexpr const char x[]

namespace laboot::pre {
    _LABOOT_DATA volatile u8_t *SERIAL_BASE = reinterpret_cast<volatile u8_t *>(0x1fe001e0ULL);

    _LABOOT_TEXT void serial_putc(char ch) {
        while ((SERIAL_BASE[5] & 0x20) == 0) {
        }
        SERIAL_BASE[0] = static_cast<u8_t>(ch);
    }

    _LABOOT_TEXT void serial_puts(const char *str) {
        for (const char *p = str; *p != '\0'; ++p) {
            serial_putc(*p);
        }
    }
}  // namespace laboot::pre

namespace laboot::msg::pre {
    _LABOOT_STRING(LABOOT_BOOT_MSG)  = "LABOOT引导程序启动!\n";
    _LABOOT_STRING(LABOOT_PANIC_MSG) = "LABOOT引导程序发生错误, 无法继续执行!\n";
    _LABOOT_STRING(LABOOT_INVALID_KERNEL_SIZE_MSG) = "错误: LABOOT 内核大小超过 32MB 限制\n";
    _LABOOT_STRING(LABOOT_INVALID_PAGING_SIZE_MSG) = "错误: LABOOT 分页保留区小于 128KB 限制\n";
    _LABOOT_STRING(LABOOT_MISALIGNED_PAGING_MSG) = "错误: LABOOT 分页保留区未对齐到 4KB\n";
    _LABOOT_STRING(LABOOT_BOUNDARY_MISALIGNED_MSG) = "错误: LABOOT 2MB 映射边界未对齐\n";
    _LABOOT_STRING(LABOOT_1G_MISALIGNED_MSG)       = "错误: LABOOT 1GB 映射边界未对齐\n";
    _LABOOT_STRING(LA_PAGE_ALLOC_OVERFLOW_MSG)     = "错误: LABOOT 分页保留区耗尽\n";
    _LABOOT_STRING(LABOOT_CHECK_PASS_MSG)          = "LABOOT检查通过!\n";
    _LABOOT_STRING(LABOOT_PAGING_READY_MSG)        = "LABOOT页表设置完成!\n";
    _LABOOT_STRING(LABOOT_INVALID_DTB_MAGIC_MSG)   = "错误: DTB魔数不正确\n";
    _LABOOT_STRING(LABOOT_INVALID_DTB_SIZE_MSG)    = "错误: DTB大小超过限制\n";
    _LABOOT_STRING(LABOOT_MISALIGNED_KPA_MSG)      = "错误: KPA区域映射失效!\n";
}  // namespace laboot::msg::pre

namespace laboot {
    using namespace pre;
    using namespace msg::pre;

    extern "C" [[noreturn]]
    void _laboot_post_start(addr_t boot_info_ptr, addr_t reclaimable_cursor);

    extern "C" _LABOOT_BSS addr_t __laboot_bsp_phys_id       = 0;
    extern "C" _LABOOT_BSS addr_t __laboot_cmdline_phys      = 0;
    extern "C" _LABOOT_BSS addr_t __laboot_system_table_phys = 0;

    _LABOOT_BSS addr_t reclaimable_cursor = 0;
    _LABOOT_BSS addr_t reclaimable_limit  = 0;
    _LABOOT_BSS LabootPagingSetup setup{};
    _LABOOT_BSS LabootInfo boot_info{};

    constexpr addr_t PA_START         = 0x00000000;
    constexpr addr_t PA_LIMIT         = 0x40000000;
    constexpr addr_t KERNEL_PHY_BASE  = 0x00300000;
    constexpr addr_t KERNEL_VIRT_BASE = 0xffffffff80300000ULL;
    extern "C" _LABOOT_TEXT [[noreturn]] void _laboot_panic() {
        serial_puts(LABOOT_PANIC_MSG);
        while (true) {
        }
    }

#define LABOOT_PANIC(x)  \
    do {                 \
        serial_puts(x);  \
        _laboot_panic(); \
    } while (0)

    _LABOOT_TEXT void page_zero(addr_t pa) {
        auto *page = reinterpret_cast<byte *>(pa);
        for (size_t i = 0; i < PAGE_SIZE; ++i) {
            page[i] = 0;
        }
    }

    _LABOOT_TEXT addr_t page_alloc() {
        addr_t current = reclaimable_cursor;
        if ((current & (PAGE_TABLE_ALIGNMENT - 1)) != 0) {
            LABOOT_PANIC(LABOOT_MISALIGNED_PAGING_MSG);
        }
        if (current + PAGE_SIZE > reclaimable_limit) {
            LABOOT_PANIC(LA_PAGE_ALLOC_OVERFLOW_MSG);
        }
        reclaimable_cursor = current + PAGE_SIZE;
        page_zero(current);
        return current;
    }

    _LABOOT_TEXT PteType *page_table(addr_t pa) {
        // 开启分页前物理地址可直接访问，因此可将页帧地址解释为页表数组。
        return reinterpret_cast<PteType *>(pa);
    }

    _LABOOT_TEXT PteType *ensure_next_level(PteType *table, size_t index) {
        PteType &entry = table[index];
        if ((entry & PPN_MASK) == 0) {
            addr_t next_level = page_alloc();
            entry             = LA_MAKE_PDE(next_level);
        }
        return page_table(entry & PPN_MASK);
    }

    _LABOOT_TEXT PteType *ensure_path(addr_t root, const size_t vpn[PAGE_LEVELS],
                                      size_t stop_level) {
        auto *table = page_table(root);
        for (size_t level = PAGE_LEVELS - 1; level > stop_level; --level) {
            table = ensure_next_level(table, vpn[level]);
        }
        return table;
    }

    _LABOOT_TEXT void mapping_in_2m(addr_t root, addr_t va, addr_t pa) {
        if ((va & PAGING_ALIGNMENT_MASK) != 0 || (pa & PAGING_ALIGNMENT_MASK) != 0) {
            LABOOT_PANIC(LABOOT_BOUNDARY_MISALIGNED_MSG);
        }

        size_t vpn[PAGE_LEVELS];
        LA_TOVPN(vpn, va);

        auto *table   = ensure_path(root, vpn, 1);
        table[vpn[1]] = LA_MAKE_PTE(pa);
    }

    _LABOOT_TEXT void map_range_in_2m(addr_t root, addr_t va_s, addr_t va_e, addr_t pa_s) {
        addr_t va = va_s;
        addr_t pa = pa_s;
        while (va < va_e) {
            mapping_in_2m(root, va, pa);
            va += PAGE_SIZE_2M;
            pa += PAGE_SIZE_2M;
        }
    }

    _LABOOT_TEXT void map_kpa_range_in_2m(addr_t root, addr_t pa_s, addr_t pa_e) {
        if (pa_e <= pa_s) {
            return;
        }

        addr_t current = pa_s & ~static_cast<addr_t>(PAGING_ALIGNMENT_MASK);
        addr_t end = (pa_e + PAGING_ALIGNMENT_MASK) & ~static_cast<addr_t>(PAGING_ALIGNMENT_MASK);
        while (current < end) {
            mapping_in_2m(root, current + KPA_START, current);
            current += PAGE_SIZE_2M;
        }
    }

    _LABOOT_TEXT void map_identity_and_kpa(addr_t root, addr_t pa_s, addr_t pa_e) {
        if ((pa_s & PAGING_ALIGNMENT_MASK) != 0 || (pa_e & PAGING_ALIGNMENT_MASK) != 0) {
            LABOOT_PANIC(LABOOT_MISALIGNED_KPA_MSG);
        }

        map_range_in_2m(root, pa_s, pa_e, pa_s);
        map_kpa_range_in_2m(root, pa_s, pa_e);
    }

    _LABOOT_TEXT void init_boot_info(addr_t root_page_table, addr_t kernel_start,
                                     addr_t kernel_end) {
        boot_info.bsp_phys_id          = __laboot_bsp_phys_id;
        boot_info.dtb_phys             = 0;
        boot_info.dtb_virt             = 0;
        boot_info.hhdm_base            = KPA_START;
        boot_info.kernel_phys_base     = KERNEL_PHY_BASE;
        boot_info.kernel_virt_base     = KERNEL_VIRT_BASE;
        boot_info.kernel_phys_end      = kernel_end;
        boot_info.kernel_virt_end      = kernel_end + LABOOT_KVA_START;
        boot_info.root_page_table_phys = root_page_table;
        boot_info.root_page_table_virt = KPA_START + root_page_table;
        boot_info.cmdline_phys         = __laboot_cmdline_phys;
        boot_info.system_table_phys    = __laboot_system_table_phys;
        boot_info.cmdline_virt         = KPA_START + boot_info.cmdline_phys;
        boot_info.system_table_virt    = KPA_START + boot_info.system_table_phys;
        (void)kernel_start;
    }

    _LABOOT_TEXT void setup_switch_context(addr_t root) {
        // C++ 只组装切换上下文，汇编入口按固定布局写 CSR 并跨越地址空间切换。
        setup.root_page_table    = root;
        setup.pwctl0             = PWCTL0_4LEVEL;
        setup.pwctl1             = PWCTL1_4LEVEL;
        setup.stlbpgsize         = STLBPGSIZE_4K;
        setup.pgdl               = root;
        setup.pgdh               = root;
        setup.dmw0               = DMW0_CONFIG;
        setup.dmw1               = 0;
        setup.dmw2               = 0;
        setup.dmw3               = 0;
        setup.tlbrentry          = reinterpret_cast<addr_t>(&_laboot_tlb_refill);
        setup.crmd_value         = CRMD_PG;
        setup.post_entry         = reinterpret_cast<addr_t>(&_laboot_post_start);
        setup.boot_info_ptr      = reinterpret_cast<addr_t>(&boot_info);
        setup.reclaimable_cursor = reclaimable_cursor;
        setup.reserved           = 0;
    }

    extern "C" _LABOOT_TEXT LabootPagingSetup *_laboot_setup() {
        serial_puts(LABOOT_BOOT_MSG);

        char *paging_start = &s_laboot_reclaimable;
        char *paging_end   = &e_laboot_reclaimable;

        size_t paging_sz = static_cast<size_t>(paging_end - paging_start);
        if (paging_sz < MINIMUM_PAGING_SIZE) {
            LABOOT_PANIC(LABOOT_INVALID_PAGING_SIZE_MSG);
        }
        if ((reinterpret_cast<addr_t>(paging_start) & (PAGE_SIZE - 1)) != 0) {
            LABOOT_PANIC(LABOOT_MISALIGNED_PAGING_MSG);
        }

        // 阶段一：以链接脚本预留区作为无堆页表分配器。
        reclaimable_cursor = reinterpret_cast<addr_t>(paging_start);
        reclaimable_limit  = reinterpret_cast<addr_t>(paging_end);

        char *kernel_start = &s_laboot;
        char *kernel_end   = &ekernel_phys;
        size_t kernel_sz   = static_cast<size_t>(kernel_end - kernel_start);
        if (kernel_sz > MAXIMUM_KERNEL_SIZE) {
            LABOOT_PANIC(LABOOT_INVALID_KERNEL_SIZE_MSG);
        }

        auto kernel_start_arith = reinterpret_cast<addr_t>(kernel_start);
        auto kernel_end_arith   = reinterpret_cast<addr_t>(kernel_end);
        if ((kernel_end_arith & PAGING_ALIGNMENT_MASK) != 0) {
            LABOOT_PANIC(LABOOT_BOUNDARY_MISALIGNED_MSG);
        }

        serial_puts(LABOOT_CHECK_PASS_MSG);

        auto aligned_kernel_start =
            kernel_start_arith & ~static_cast<addr_t>(PAGING_ALIGNMENT_MASK);
        auto kernel_kva_limit = kernel_end_arith + LABOOT_KVA_START;
        auto root_page_table  = page_alloc();

        // 阶段二：同时保留切换所需恒等映射，并建立 KPA 与内核高半区映射。
        map_identity_and_kpa(root_page_table, PA_START, PA_LIMIT);
        map_range_in_2m(root_page_table, aligned_kernel_start + LABOOT_KVA_START, kernel_kva_limit,
                        aligned_kernel_start);

        init_boot_info(root_page_table, aligned_kernel_start, kernel_end_arith);

        if (boot_info.system_table_phys != 0) {
            map_kpa_range_in_2m(root_page_table, boot_info.system_table_phys,
                                boot_info.system_table_phys + PAGE_SIZE);
        }
        if (boot_info.cmdline_phys != 0) {
            map_kpa_range_in_2m(root_page_table, boot_info.cmdline_phys,
                                boot_info.cmdline_phys + PAGE_SIZE);
        }

        // 阶段三：将页表根、CSR 配置和高半区入口打包给汇编 trampoline。
        setup_switch_context(root_page_table);

        serial_puts(LABOOT_PAGING_READY_MSG);
        return &setup;
    }
}  // namespace laboot
