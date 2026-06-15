/**
 * @file sbi_boot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI 启动代码
 * @version alpha-1.0.0
 * @date 2026-06-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/sbi/sbi_paging.h>
#include <sustcore/addr.h>

#include <cstddef>

#define _SBI_FUNCTION   SECTION(".sbi_boot.text")
#define _SBI_DATA       SECTION(".sbi_boot.data")
#define _SBI_RODATA     SECTION(".sbi_boot.rodata")
#define _SBI_STRING(x)  _SBI_RODATA constexpr const char x[]
#define _SBI_STRLEN(x)  (sizeof(x) - 1)
#define _SBI_WRITE(str) _sbi_write(_SBI_STRLEN(str), str)

namespace sbi {
    _SBI_STRING(SBI_BOOT_MSG) = "SBI引导程序启动!\n";
    _SBI_STRING(SBI_PANIC_MSG) = "SBI引导程序发生错误，无法继续执行!\n";
    _SBI_STRING(SBI_INVALID_KERNEL_SIZE_MSG) =
        "错误: 内核大小超过 32MB  限制\n";
    _SBI_STRING(SBI_INVALID_PAGING_SIZE_MSG) = "错误: 分页部分小于 64KB 限制\n";
    _SBI_STRING(SBI_MISALIGNED_PAGING_MSG) = "错误: 分页部分地址未对齐到4KB\n";
    _SBI_STRING(SBI_BOUNDARY_MISALIGNED_MSG) =
        "错误: 内核末尾地址未对齐到2MB\n";
    _SBI_STRING(SBI_1G_MISALIGNED_MSG) = "错误: 1GB 映射地址未对齐\n";
    _SBI_STRING(SBI_PAGE_ALLOC_OVERFLOW_MSG) = "错误: SBI 分页保留区耗尽\n";
    _SBI_STRING(SBI_CHECK_PASS_MSG) = "SBI引导程序检查通过!\n";
    _SBI_STRING(SBI_PAGING_SETUP_COMPLETE_MSG) = "SBI分页设置完成!\n";
    _SBI_STRING(SBI_INVALID_DTB_MAGIC_MSG)     = "错误: DTB魔数不正确\n";
    _SBI_STRING(SBI_INVALID_DTB_SIZE_MSG) = "错误: DTB大小超过限制\n";
    _SBI_STRING(SBI_MISALIGNED_KPA_MSG)   = "错误: KPA区域映射失效!\n";
}  // namespace sbi

namespace sbi {
    extern "C" size_t __sbi_boot_hart_id;
    extern "C" addr_t __sbi_dtb_phys;
    extern "C" addr_t __sbi_reclaimable_cursor;
    static_assert(sizeof(__sbi_boot_hart_id) == 8, "类型大小不匹配");
    static_assert(sizeof(__sbi_dtb_phys) == 8, "类型大小不匹配");

    extern "C" _SBI_FUNCTION void _sbi_write(size_t len, const char *buf);

    _SBI_FUNCTION void _sbi_panic() {
        _SBI_WRITE(SBI_PANIC_MSG);
        while (true);
    }

#define _SBI_PANIC(x)  \
    do {               \
        _SBI_WRITE(x); \
        _sbi_panic();  \
    } while (0)

    static _SBI_DATA addr_t reclaimable_cursor;
    static _SBI_DATA addr_t reclaimable_limit;
    static _SBI_DATA addr_t root_page_table;
    static _SBI_DATA addr_t kernel_kva_limit;

    // 将初始 8000'0000 ~ C000'0000 区域 (1GB) 映射到 PA 与 KPA 中
    static constexpr addr_t PA_START = 0x80000000;
    static constexpr addr_t PA_LIMIT = 0xC0000000;

    _SBI_FUNCTION void page_zero(addr_t pa) {
        auto *page = reinterpret_cast<byte *>(pa);
        for (size_t i = 0; i < PAGE_SIZE; ++i) {
            page[i] = 0;
        }
    }

    _SBI_FUNCTION addr_t page_alloc() {
        addr_t current = reclaimable_cursor;
        if ((current & (PAGE_TABLE_ALIGNMENT - 1)) != 0) {
            _SBI_PANIC(SBI_MISALIGNED_PAGING_MSG);
        }
        if (current + PAGE_SIZE > reclaimable_limit) {
            _SBI_PANIC(SBI_PAGE_ALLOC_OVERFLOW_MSG);
        }
        reclaimable_cursor = current + PAGE_SIZE;
        page_zero(current);
        return current;
    }

    _SBI_FUNCTION pte_t *page_table(addr_t pa) {
        return reinterpret_cast<pte_t *>(pa);
    }

    _SBI_FUNCTION pte_t *ensure_next_level(pte_t *table, size_t index) {
        pte_t &entry = table[index];
        if ((entry & PDE_BASE) == 0) {
            addr_t next_level = page_alloc();
            entry             = MAKE_PDE(next_level);
        }
        return page_table((entry & PPN_MASK) >> PPN_SHIFT << 12);
    }

    _SBI_FUNCTION void mapping_in_2m(addr_t root, addr_t va, addr_t pa) {
        if ((va & PAGING_ALIGNMENT_MASK) != 0 ||
            (pa & PAGING_ALIGNMENT_MASK) != 0)
        {
            _SBI_PANIC(SBI_BOUNDARY_MISALIGNED_MSG);
        }

        size_t vpn[3];
        TOVPN(vpn, va);

        auto *l0   = page_table(root);
        auto *l1   = ensure_next_level(l0, vpn[2]);
        l1[vpn[1]] = MAKE_PTE(pa);
    }

    _SBI_FUNCTION void mapping_in_1g(addr_t root, addr_t va, addr_t pa) {
        if ((va & PAGE_SIZE_1G_MASK) != 0 || (pa & PAGE_SIZE_1G_MASK) != 0) {
            _SBI_PANIC(SBI_1G_MISALIGNED_MSG);
        }

        size_t vpn[3];
        TOVPN(vpn, va);

        auto *l0   = page_table(root);
        l0[vpn[2]] = MAKE_PTE(pa);
    }

    _SBI_FUNCTION void map_range_in_2m(addr_t root, addr_t va_s, addr_t va_e,
                                       addr_t pa_s) {
        addr_t va = va_s;
        addr_t pa = pa_s;
        while (va < va_e) {
            mapping_in_2m(root, va, pa);
            va += PAGE_SIZE_2M;
            pa += PAGE_SIZE_2M;
        }
    }

    _SBI_FUNCTION void map_identity_and_kpa(addr_t root, addr_t pa_s,
                                            addr_t pa_e) {
        addr_t current = pa_s;

        if ((pa_s & PAGE_SIZE_1G_MASK) != 0 || (pa_e & PAGE_SIZE_1G_MASK) != 0)
        {
            _SBI_PANIC(SBI_MISALIGNED_KPA_MSG);
        }

        while (current + PAGE_SIZE_1G <= pa_e) {
            mapping_in_1g(root, current, current);
            mapping_in_1g(root, current + KPA_OFFSET, current);
            current += PAGE_SIZE_1G;
        }
    }

    _SBI_FUNCTION size_t calc_satp() {
        return SATP_SV39_BASE | TO_PPN(root_page_table);
    }

    _SBI_FUNCTION qword dtb_byte(size_t offset) {
        return *(byte *)(__sbi_dtb_phys + offset);
    }

#define DTB_BYTE(x)  dtb_byte(x)
#define DTB_WORD(x)  (DTB_BYTE(x) << 8 | DTB_BYTE((x) + 1))
#define DTB_DWORD(x) (DTB_WORD(x) << 16 | DTB_WORD((x) + 2))

    extern "C" _SBI_FUNCTION size_t _sbi_setup() {
        _SBI_WRITE(SBI_BOOT_MSG);

        char *paging_start = &s_sbi_reclaimable;
        char *paging_end   = &e_sbi_reclaimable;

        size_t paging_size = paging_end - paging_start;
        if (paging_size < MINIMUM_PAGING_SIZE) {
            _SBI_PANIC(SBI_INVALID_PAGING_SIZE_MSG);
        }
        if ((reinterpret_cast<addr_t>(paging_start) & (PAGE_SIZE - 1)) != 0) {
            _SBI_PANIC(SBI_MISALIGNED_PAGING_MSG);
        }

        // 设置页表分配起始与终点位置
        reclaimable_cursor = reinterpret_cast<addr_t>(paging_start);
        reclaimable_limit  = reinterpret_cast<addr_t>(paging_end);

        char *kernel_start = &s_sbi;
        char *kernel_end   = &ekernel_phys;
        size_t kernel_size = kernel_end - kernel_start;

        if (kernel_size > MAXIMUM_KERNEL_SIZE) {
            _SBI_PANIC(SBI_INVALID_KERNEL_SIZE_MSG);
        }

        auto kernel_end_arith = reinterpret_cast<addr_t>(kernel_end);
        if ((kernel_end_arith & PAGING_ALIGNMENT_MASK) != 0) {
            _SBI_PANIC(SBI_BOUNDARY_MISALIGNED_MSG);
        }

        auto kernel_start_arith   = reinterpret_cast<addr_t>(kernel_start);
        auto aligned_kernel_start = kernel_start_arith & ~PAGING_ALIGNMENT_MASK;

        _SBI_WRITE(SBI_CHECK_PASS_MSG);

        dword magic = DTB_DWORD(0);
        if (magic != 0xD00DFEED) {
            _SBI_PANIC(SBI_INVALID_DTB_MAGIC_MSG);
        }
        dword dtb_size = DTB_DWORD(4);
        if (dtb_size > MAXIMUM_DTB_SIZE) {
            _SBI_PANIC(SBI_INVALID_DTB_SIZE_MSG);
        }

        root_page_table  = page_alloc();
        kernel_kva_limit = kernel_end_arith + KVA_OFFSET;

        // 设置恒等映射与kpa映射
        map_identity_and_kpa(root_page_table, PA_START, PA_LIMIT);
        // 映射内核空间到高地址区
        map_range_in_2m(root_page_table, aligned_kernel_start + KVA_OFFSET,
                        kernel_kva_limit, aligned_kernel_start);

        auto aligned_dtb_start = __sbi_dtb_phys & ~PAGING_ALIGNMENT_MASK;
        auto dtb_end           = __sbi_dtb_phys + dtb_size;
        auto aligned_dtb_end =
            (dtb_end + PAGING_ALIGNMENT_MASK) & ~PAGING_ALIGNMENT_MASK;
        map_range_in_2m(root_page_table, aligned_dtb_start + KVA_OFFSET,
                        aligned_dtb_end + KVA_OFFSET, aligned_dtb_start);

        __sbi_reclaimable_cursor = reclaimable_cursor;
        _SBI_WRITE(SBI_PAGING_SETUP_COMPLETE_MSG);
        return calc_satp();
    }
}  // namespace sbi
