/**
 * @file post_boot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI 平台信息与通用启动上下文适配
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/boot.h>
#include <boot/common/bootinfo.h>
#include <boot/sbi/arch/riscv64/paging.h>
#include <sbi/sbi.h>
#include <tay/attribute.h>

#include <cstring>

#define SBI_BOOT_POST_STRING(x) SBI_BOOT_POST_RODATA constexpr const char x[]

namespace sbi {
    extern "C" [[noreturn]] void __bsp_start(size_t bsp_hwid, const BootInfoHeader *bootinfo);

    SBI_BOOT_POST_STRING(SBI_POST_BOOT_MSG)          = "SBI引导程序第二部分启动!\n";
    SBI_BOOT_POST_STRING(SBI_KERNEL_ENTRY_MSG)       = "SBI引导程序进入内核入口!\n";
    SBI_BOOT_POST_STRING(SBI_BOOTINFO_OVERFLOW_MSG)  = "错误: BootInfo 区域数量超限\n";
    SBI_BOOT_POST_STRING(SBI_BOOTINFO_ALLOC_MSG)     = "错误: SBI reclaimable 区域不足\n";
    SBI_BOOT_POST_STRING(SBI_INVALID_DTB_MSG)        = "错误: FDT 无效\n";
    SBI_BOOT_POST_STRING(SBI_BOOTINFO_TOO_LARGE_MSG) = "错误: BootInfo 超过 128KB 限制\n";
    SBI_BOOT_POST_STRING(SBI_BOOTINFO_INVALID_REGION_MSG) =
        "错误: BootInfo 存在无效或未页对齐区域\n";

    SBI_BOOT_POST_TEXT void sbi_writes(const char *str) noexcept {
        const int len = strlen(str);
        for (int idx = 0; idx < len; ++idx) sbi_dbcn_console_write_byte(str[idx]);
    }

    [[noreturn]] SBI_BOOT_POST_TEXT void post_panic(const char *msg) noexcept {
        sbi_writes(msg);
        while (true) {
        }
    }

    [[noreturn]] SBI_BOOT_POST_TEXT void bootinfo_panic(boot::BootInfoBuildError error) noexcept {
        switch (error) {
            case boot::BootInfoBuildError::INVALID_DTB: post_panic(SBI_INVALID_DTB_MSG);
            case boot::BootInfoBuildError::INVALID_REGION:
                post_panic(SBI_BOOTINFO_INVALID_REGION_MSG);
            case boot::BootInfoBuildError::REGION_CAPACITY:  post_panic(SBI_BOOTINFO_OVERFLOW_MSG);
            case boot::BootInfoBuildError::OUTPUT_TOO_LARGE: post_panic(SBI_BOOTINFO_TOO_LARGE_MSG);
            case boot::BootInfoBuildError::OUTPUT_CAPACITY:  post_panic(SBI_BOOTINFO_ALLOC_MSG);
        }
        __builtin_unreachable();
    }

    using BootInfoBuilderType = boot::BootInfoBuilder<MAX_BOOTINFO_REGIONS, bootinfo_panic>;
    // 三组区域暂存数组大于 SBI 启动栈，必须保存在 post-boot 私有数据段中。
    static SBI_BOOT_POST_BSS BootInfoBuilderType bootinfo_builder;

    [[nodiscard]] SBI_BOOT_POST_TEXT addr_t kva_to_pa(const char *ptr) noexcept {
        return reinterpret_cast<addr_t>(ptr) - KVA_START;
    }

    [[nodiscard]] SBI_BOOT_POST_TEXT BootInfoHeader *build_bootinfo(addr_t dtb_ptr,
                                                                    addr_t cursor) noexcept {
        bootinfo_builder.reset(reinterpret_cast<const void *>(dtb_ptr), 1);
        bootinfo_builder.collect_fdt_areas();
        const auto dtb_sz = bootinfo_builder.dtb_sz();
        if (dtb_sz > addr_t(-1) - dtb_ptr)
            bootinfo_panic(boot::BootInfoBuildError::INVALID_REGION);
        bootinfo_builder.append_reserved(
            page_align_outward(PhyArea(PhyAddr(dtb_ptr), PhyAddr(dtb_ptr + dtb_sz))),
            MemoryType::BOOT_RECLAIMABLE);
        bootinfo_builder.append_memory(
            PhyArea(PhyAddr(kva_to_pa(&s_sbi_kva)), PhyAddr(kva_to_pa(&s_sbi_reclaimable_kva))));
        bootinfo_builder.append_reserved(PhyArea(PhyAddr(kva_to_pa(&s_sbi_reclaimable_kva)),
                                                 PhyAddr(kva_to_pa(&e_sbi_reclaimable_kva))),
                                         MemoryType::BOOT_RECLAIMABLE);
        bootinfo_builder.append_reserved(
            PhyArea(PhyAddr(kva_to_pa(&e_sbi_reclaimable_kva)), PhyAddr(kva_to_pa(&s_init))),
            MemoryType::RESERVED);
        bootinfo_builder.append_reserved(
            PhyArea(PhyAddr(kva_to_pa(&s_init)), PhyAddr(kva_to_pa(&e_init))),
            MemoryType::BOOT_RECLAIMABLE);
        bootinfo_builder.append_reserved(
            PhyArea(PhyAddr(kva_to_pa(&e_init)), PhyAddr(kva_to_pa(&ekernel))),
            MemoryType::RESERVED);
        return bootinfo_builder.write(cursor, kva_to_pa(&e_sbi_reclaimable_kva),
                                      PAGE_TABLE_ALIGNMENT, PhyAddr(dtb_ptr));
    }

    extern "C" SBI_BOOT_POST_TEXT void _sbi_post_start(size_t hart_id, addr_t dtb_ptr,
                                                       addr_t reclaimable_cursor) {
        sbi_writes(SBI_POST_BOOT_MSG);
        auto *bootinfo    = build_bootinfo(dtb_ptr, reclaimable_cursor);
        bootinfo->hart_id = hart_id;
        sbi_writes(SBI_KERNEL_ENTRY_MSG);
        __bsp_start(hart_id, bootinfo);
    }
}  // namespace sbi
