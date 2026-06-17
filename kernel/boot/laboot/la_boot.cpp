/**
 * @file la_boot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief laboot 启动第一阶段: 构造页表并准备分页切换描述
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/laboot/la_paging.h>
#include <arch/loongarch64/csrnum.h>
#include <sus/types.h>

#include <cstddef>
#include <cstdint>

#define _LABOOT_FUNCTION  SECTION(".laboot.text")
#define _LABOOT_DATA      SECTION(".laboot.data")
#define _LABOOT_RODATA    SECTION(".laboot.rodata")
#define _LABOOT_STRING(x) _LABOOT_RODATA constexpr const char x[]

namespace laboot::pre {
    _LABOOT_DATA volatile uint8_t *SERIAL_BASE =
        reinterpret_cast<volatile uint8_t *>(0x1fe001e0ULL);

    _LABOOT_FUNCTION void serial_putc(char ch) {
        while ((SERIAL_BASE[5] & 0x20) == 0) {
        }
        SERIAL_BASE[0] = static_cast<uint8_t>(ch);
    }

    _LABOOT_FUNCTION void serial_puts(const char *str) {
        for (const char *p = str; *p != '\0'; ++p) {
            serial_putc(*p);
        }
    }
}  // namespace laboot::pre

namespace laboot::msg::pre {
    _LABOOT_STRING(LABOOT_BOOT_MSG) = "LABOOT引导程序启动!\n";
    _LABOOT_STRING(LABOOT_PANIC_MSG) =
        "LABOOT引导程序发生错误，无法继续执行!\n";
    _LABOOT_STRING(LABOOT_INVALID_KERNEL_SIZE_MSG) =
        "错误: LABOOT 内核大小超过 32MB 限制\n";
    _LABOOT_STRING(LABOOT_INVALID_PAGING_SIZE_MSG) =
        "错误: LABOOT 分页保留区小于 128KB 限制\n";
    _LABOOT_STRING(LABOOT_MISALIGNED_PAGING_MSG) =
        "错误: LABOOT 分页保留区未对齐到 4KB\n";
    _LABOOT_STRING(LABOOT_BOUNDARY_MISALIGNED_MSG) =
        "错误: LABOOT 2MB 映射边界未对齐\n";
    _LABOOT_STRING(LABOOT_1G_MISALIGNED_MSG) =
        "错误: LABOOT 1GB 映射边界未对齐\n";
    _LABOOT_STRING(LA_PAGE_ALLOC_OVERFLOW_MSG) =
        "错误: LABOOT 分页保留区耗尽\n";
    _LABOOT_STRING(LABOOT_CHECK_PASS_MSG)   = "LABOOT检查通过!\n";
    _LABOOT_STRING(LABOOT_PAGING_READY_MSG) = "LABOOT页表设置完成!\n";
    _LABOOT_STRING(LABOOT_INVALID_DTB_MAGIC_MSG) = "错误: DTB魔数不正确\n";
    _LABOOT_STRING(LABOOT_INVALID_DTB_SIZE_MSG) = "错误: DTB大小超过限制\n";
    _LABOOT_STRING(LABOOT_MISALIGNED_KPA_MSG) = "错误: KPA区域映射失效!\n";
}  // namespace laboot::msg::pre

namespace laboot {
    using namespace pre;
    using namespace msg::pre;

    extern "C" [[noreturn]]
    void _laboot_post_start(addr_t boot_info_ptr, addr_t reclaimable_cursor);

    extern "C" _LABOOT_DATA addr_t __laboot_bsp_phys_id         = 0;
    extern "C" _LABOOT_DATA addr_t __laboot_cmdline_phys        = 0;
    extern "C" _LABOOT_DATA addr_t __laboot_system_table_phys   = 0;

    _LABOOT_DATA addr_t reclaimable_cursor = 0;
    _LABOOT_DATA addr_t reclaimable_limit  = 0;
    _LABOOT_DATA LabootPagingSetup setup{};
    _LABOOT_DATA LabootInfo boot_info{};

    constexpr addr_t PA_START         = 0x00000000;
    constexpr addr_t PA_LIMIT         = 0x40000000;
    constexpr addr_t KERNEL_PHY_BASE  = 0x00300000;
    constexpr addr_t KERNEL_VIRT_BASE = 0xffffffff80300000ULL;
    extern "C" _LABOOT_FUNCTION [[noreturn]] void _laboot_panic() {
        serial_puts(LABOOT_PANIC_MSG);
        while (true) {
        }
    }

#define LABOOT_PANIC(x)  \
    do {                 \
        serial_puts(x);  \
        _laboot_panic(); \
    } while (0)

    _LABOOT_FUNCTION void page_zero(addr_t pa) {
        auto *page = reinterpret_cast<byte *>(pa);
        for (size_t i = 0; i < PAGE_SIZE; ++i) {
            page[i] = 0;
        }
    }

    _LABOOT_FUNCTION addr_t page_alloc() {
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

    _LABOOT_FUNCTION pte_t *page_table(addr_t pa) {
        return reinterpret_cast<pte_t *>(pa);
    }

    _LABOOT_FUNCTION pte_t *ensure_next_level(pte_t *table, size_t index) {
        pte_t &entry = table[index];
        if ((entry & PPN_MASK) == 0) {
            addr_t next_level = page_alloc();
            entry             = LA_MAKE_PDE(next_level);
        }
        return page_table(entry & PPN_MASK);
    }

    _LABOOT_FUNCTION pte_t *ensure_path(addr_t root,
                                        const size_t vpn[PAGE_LEVELS],
                                        size_t stop_level) {
        auto *table = page_table(root);
        for (size_t level = PAGE_LEVELS - 1; level > stop_level; --level) {
            table = ensure_next_level(table, vpn[level]);
        }
        return table;
    }

    _LABOOT_FUNCTION void mapping_in_2m(addr_t root, addr_t va, addr_t pa) {
        if ((va & PAGING_ALIGNMENT_MASK) != 0 ||
            (pa & PAGING_ALIGNMENT_MASK) != 0)
        {
            LABOOT_PANIC(LABOOT_BOUNDARY_MISALIGNED_MSG);
        }

        size_t vpn[PAGE_LEVELS];
        LA_TOVPN(vpn, va);

        auto *table   = ensure_path(root, vpn, 1);
        table[vpn[1]] = LA_MAKE_PTE(pa);
    }

    _LABOOT_FUNCTION void map_range_in_2m(addr_t root, addr_t va_s, addr_t va_e,
                                          addr_t pa_s) {
        addr_t va = va_s;
        addr_t pa = pa_s;
        while (va < va_e) {
            mapping_in_2m(root, va, pa);
            va += PAGE_SIZE_2M;
            pa += PAGE_SIZE_2M;
        }
    }

    _LABOOT_FUNCTION void map_kpa_range_in_2m(addr_t root, addr_t pa_s,
                                              addr_t pa_e) {
        if (pa_e <= pa_s) {
            return;
        }

        addr_t current = pa_s & ~static_cast<addr_t>(PAGING_ALIGNMENT_MASK);
        addr_t end     = (pa_e + PAGING_ALIGNMENT_MASK) &
                     ~static_cast<addr_t>(PAGING_ALIGNMENT_MASK);
        while (current < end) {
            mapping_in_2m(root, current + KPA_OFFSET, current);
            current += PAGE_SIZE_2M;
        }
    }

    _LABOOT_FUNCTION void map_identity_and_kpa(addr_t root, addr_t pa_s,
                                               addr_t pa_e) {
        if ((pa_s & PAGING_ALIGNMENT_MASK) != 0 ||
            (pa_e & PAGING_ALIGNMENT_MASK) != 0)
        {
            LABOOT_PANIC(LABOOT_MISALIGNED_KPA_MSG);
        }

        map_range_in_2m(root, pa_s, pa_e, pa_s);
        map_kpa_range_in_2m(root, pa_s, pa_e);
    }

    _LABOOT_FUNCTION void init_boot_info(addr_t root_page_table,
                                         addr_t kernel_start,
                                         addr_t kernel_end) {
        boot_info.bsp_phys_id          = __laboot_bsp_phys_id;
        boot_info.dtb_phys             = 0;
        boot_info.dtb_virt             = 0;
        boot_info.hhdm_base            = KPA_OFFSET;
        boot_info.kernel_phys_base     = KERNEL_PHY_BASE;
        boot_info.kernel_virt_base     = KERNEL_VIRT_BASE;
        boot_info.kernel_phys_end      = kernel_end;
        boot_info.kernel_virt_end      = kernel_end + LABOOT_KVA_OFFSET;
        boot_info.root_page_table_phys = root_page_table;
        boot_info.root_page_table_virt = KPA_OFFSET + root_page_table;
        boot_info.cmdline_phys         = __laboot_cmdline_phys;
        boot_info.system_table_phys    = __laboot_system_table_phys;
        boot_info.cmdline_virt         = KPA_OFFSET + boot_info.cmdline_phys;
        boot_info.system_table_virt    = KPA_OFFSET + boot_info.system_table_phys;
        (void)kernel_start;
    }

    _LABOOT_FUNCTION void setup_switch_context(addr_t root) {
        setup.root_page_table = root;
        setup.pwctl0          = PWCTL0_4LEVEL;
        setup.pwctl1          = PWCTL1_4LEVEL;
        setup.stlbpgsize      = STLBPGSIZE_4K;
        setup.pgdl            = root;
        setup.pgdh            = root;
        setup.dmw0            = DMW0_CONFIG;
        setup.dmw1            = 0;
        setup.dmw2            = 0;
        setup.dmw3            = 0;
        setup.tlbrentry       = reinterpret_cast<addr_t>(&_laboot_tlb_refill);
        setup.crmd_value      = CRMD_PG;
        setup.post_entry      = reinterpret_cast<addr_t>(&_laboot_post_start);
        setup.boot_info_ptr   = reinterpret_cast<addr_t>(&boot_info);
        setup.reclaimable_cursor = reclaimable_cursor;
        setup.reserved           = 0;
    }

    extern "C" _LABOOT_FUNCTION LabootPagingSetup *_laboot_setup() {
        serial_puts(LABOOT_BOOT_MSG);

        char *paging_start = &s_laboot_reclaimable;
        char *paging_end   = &e_laboot_reclaimable;

        size_t paging_size = static_cast<size_t>(paging_end - paging_start);
        if (paging_size < MINIMUM_PAGING_SIZE) {
            LABOOT_PANIC(LABOOT_INVALID_PAGING_SIZE_MSG);
        }
        if ((reinterpret_cast<addr_t>(paging_start) & (PAGE_SIZE - 1)) != 0) {
            LABOOT_PANIC(LABOOT_MISALIGNED_PAGING_MSG);
        }

        reclaimable_cursor = reinterpret_cast<addr_t>(paging_start);
        reclaimable_limit  = reinterpret_cast<addr_t>(paging_end);

        char *kernel_start = &s_laboot;
        char *kernel_end   = &ekernel_phys;
        size_t kernel_size = static_cast<size_t>(kernel_end - kernel_start);
        if (kernel_size > MAXIMUM_KERNEL_SIZE) {
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
        auto kernel_kva_limit = kernel_end_arith + LABOOT_KVA_OFFSET;
        auto root_page_table  = page_alloc();

        map_identity_and_kpa(root_page_table, PA_START, PA_LIMIT);
        map_range_in_2m(root_page_table,
                        aligned_kernel_start + LABOOT_KVA_OFFSET,
                        kernel_kva_limit, aligned_kernel_start);

        init_boot_info(root_page_table, aligned_kernel_start, kernel_end_arith);

        if (boot_info.system_table_phys != 0) {
            map_kpa_range_in_2m(root_page_table, boot_info.system_table_phys,
                                boot_info.system_table_phys + PAGE_SIZE);
        }
        if (boot_info.cmdline_phys != 0) {
            map_kpa_range_in_2m(root_page_table, boot_info.cmdline_phys,
                                boot_info.cmdline_phys + PAGE_SIZE);
        }

        setup_switch_context(root_page_table);

        serial_puts(LABOOT_PAGING_READY_MSG);
        return &setup;
    }
}  // namespace laboot
