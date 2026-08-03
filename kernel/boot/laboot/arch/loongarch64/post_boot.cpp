/**
 * @file post_boot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LABOOT 平台信息与通用启动上下文适配
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/boot.h>
#include <boot/common/bootinfo_builder.h>
#include <boot/laboot/arch/loongarch64/early_paging.h>
#include <tay/bits.h>

#define _LABOOT_POST_STRING(x) _LABOOT_POST_RODATA constexpr const char x[]

namespace laboot::post {
    _LABOOT_POST_DATA volatile u8_t *SERIAL_BASE = reinterpret_cast<volatile u8_t *>(0x1fe001e0ULL);

    _LABOOT_POST_TEXT void serial_putc(char ch) noexcept {
        while ((SERIAL_BASE[5] & 0x20) == 0) {
        }
        SERIAL_BASE[0] = static_cast<u8_t>(ch);
    }

    _LABOOT_POST_TEXT void serial_puts(const char *str) noexcept {
        for (const char *ptr = str; *ptr != '\0'; ++ptr) serial_putc(*ptr);
    }
}  // namespace laboot::post

namespace laboot::msg::post {
    _LABOOT_POST_STRING(POST_MSG)               = "LABOOT启动第二阶段!\n";
    _LABOOT_POST_STRING(KERNEL_ENTRY_MSG)       = "LABOOT进入内核入口!\n";
    _LABOOT_POST_STRING(BOOTINFO_OVERFLOW_MSG)  = "错误: BootInfo 区域数量超限\n";
    _LABOOT_POST_STRING(BOOTINFO_ALLOC_MSG)     = "错误: LABOOT reclaimable 区域不足\n";
    _LABOOT_POST_STRING(INVALID_DTB_MSG)        = "错误: FDT 无效\n";
    _LABOOT_POST_STRING(BOOTINFO_TOO_LARGE_MSG) = "错误: BootInfo 超过 128KB 限制\n";
    _LABOOT_POST_STRING(BOOTINFO_INVALID_REGION_MSG) = "错误: BootInfo 存在无效或未页对齐区域\n";
    _LABOOT_POST_STRING(FDT_FOUND_MSG) = "LABOOT 成功校验并处理启动数据\n";
}  // namespace laboot::msg::post

namespace laboot {
    using namespace msg::post;
    using namespace post;

    extern "C" [[noreturn]] void __bsp_start(size_t bsp_hwid, const BootInfoHeader *bootinfo);

    constexpr size_t EFI_MAX_CONFIG_TABLES = 4096;

    struct EfiGuid {
        u32_t data1;
        u16_t data2;
        u16_t data3;
        u8_t data4[8];
    };

    struct EfiConfigurationTable {
        EfiGuid vendor_guid;
        void *vendor_table;
    };

    struct EfiSystemTable {
        u64_t hdr[3];
        void *firmware_vendor;
        u32_t firmware_revision;
        void *console_in_handle;
        void *con_in;
        void *console_out_handle;
        void *con_out;
        void *standard_error_handle;
        void *std_err;
        void *runtime_services;
        void *boot_services;
        size_t number_of_table_entries;
        EfiConfigurationTable *configuration_table;
    };

    constexpr EfiGuid EFI_DTB_TABLE_GUID{
        .data1 = 0xb1b621d5,
        .data2 = 0xf19c,
        .data3 = 0x41a5,
        .data4 = {0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0},
    };

    [[noreturn]] _LABOOT_POST_TEXT void post_panic(const char *msg) noexcept {
        serial_puts(msg);
        while (true) {
        }
    }

    [[noreturn]] _LABOOT_POST_TEXT void bootinfo_panic(boot::BootInfoBuildError error) noexcept {
        switch (error) {
            case boot::BootInfoBuildError::INVALID_DTB:      post_panic(INVALID_DTB_MSG);
            case boot::BootInfoBuildError::INVALID_REGION:   post_panic(BOOTINFO_INVALID_REGION_MSG);
            case boot::BootInfoBuildError::REGION_CAPACITY:  post_panic(BOOTINFO_OVERFLOW_MSG);
            case boot::BootInfoBuildError::OUTPUT_TOO_LARGE: post_panic(BOOTINFO_TOO_LARGE_MSG);
            case boot::BootInfoBuildError::OUTPUT_CAPACITY:  post_panic(BOOTINFO_ALLOC_MSG);
        }
        __builtin_unreachable();
    }

    using BootInfoBuilderType = boot::BootInfoBuilder<MAX_BOOTINFO_REGIONS, bootinfo_panic>;
    // 三组区域暂存数组大于 LABOOT 启动栈，必须保存在 post-boot 私有数据段中。
    static _LABOOT_POST_BSS BootInfoBuilderType bootinfo_builder;

    [[nodiscard]] _LABOOT_POST_TEXT addr_t kva_to_pa(const char *ptr) noexcept {
        return reinterpret_cast<addr_t>(ptr) - KVA_START;
    }

    [[nodiscard]] _LABOOT_POST_TEXT void *pa_to_hhdm(addr_t paddr) noexcept {
        if (paddr == 0)
            return nullptr;
        return reinterpret_cast<void *>(paddr + KPA_START);
    }

    [[nodiscard]] _LABOOT_POST_TEXT bool guid_equal(const EfiGuid &lhs,
                                                    const EfiGuid &rhs) noexcept {
        if (lhs.data1 != rhs.data1 || lhs.data2 != rhs.data2 || lhs.data3 != rhs.data3)
            return false;
        for (size_t idx = 0; idx < sizeof(lhs.data4); ++idx)
            if (lhs.data4[idx] != rhs.data4[idx])
                return false;
        return true;
    }

    [[nodiscard]] _LABOOT_POST_TEXT EfiConfigurationTable *config_tables(LabootInfo &boot_info,
                                                                         size_t &cnt) noexcept {
        cnt                = 0;
        auto *system_table = static_cast<EfiSystemTable *>(pa_to_hhdm(boot_info.system_table_phys));
        if (system_table == nullptr || system_table->configuration_table == nullptr ||
            system_table->number_of_table_entries == 0 ||
            system_table->number_of_table_entries > EFI_MAX_CONFIG_TABLES)
            return nullptr;
        const auto config_paddr = reinterpret_cast<addr_t>(system_table->configuration_table);
        auto *tables            = static_cast<EfiConfigurationTable *>(pa_to_hhdm(config_paddr));
        if (tables == nullptr)
            return nullptr;
        boot_info.system_table_virt = reinterpret_cast<addr_t>(system_table);
        cnt                         = system_table->number_of_table_entries;
        return tables;
    }

    [[nodiscard]] _LABOOT_POST_TEXT void *find_config_table(LabootInfo &boot_info,
                                                            const EfiGuid &guid) noexcept {
        size_t table_cnt = 0;
        auto *tables     = config_tables(boot_info, table_cnt);
        if (tables == nullptr)
            return nullptr;
        for (size_t idx = 0; idx < table_cnt; ++idx)
            if (guid_equal(tables[idx].vendor_guid, guid))
                return tables[idx].vendor_table;
        return nullptr;
    }

    _LABOOT_POST_TEXT void find_dtb_from_system_table(LabootInfo &boot_info) noexcept {
        if (boot_info.dtb_virt != 0)
            return;
        auto *dtb_table = find_config_table(boot_info, EFI_DTB_TABLE_GUID);
        if (dtb_table == nullptr)
            post_panic(INVALID_DTB_MSG);
        boot_info.dtb_phys = reinterpret_cast<addr_t>(dtb_table);
        boot_info.dtb_virt = reinterpret_cast<addr_t>(pa_to_hhdm(boot_info.dtb_phys));
        if (boot_info.dtb_virt == 0)
            post_panic(INVALID_DTB_MSG);
    }

    [[nodiscard]] _LABOOT_POST_TEXT BootInfoHeader *build_bootinfo(LabootInfo *laboot_info,
                                                                   addr_t cursor) noexcept {
        find_dtb_from_system_table(*laboot_info);
        bootinfo_builder.reset(reinterpret_cast<const void *>(laboot_info->dtb_virt), 2);
        bootinfo_builder.collect_fdt_regions();
        const auto dtb_sz = bootinfo_builder.dtb_sz();
        if (dtb_sz > addr_t(-1) - laboot_info->dtb_phys)
            bootinfo_panic(boot::BootInfoBuildError::INVALID_REGION);
        bootinfo_builder.append_reserved(
            page_align_outward(
                PhyArea(PhyAddr(laboot_info->dtb_phys), PhyAddr(laboot_info->dtb_phys + dtb_sz))),
            MemoryType::BOOT_RECLAIMABLE);
        bootinfo_builder.append_memory(PhyArea(PhyAddr(kva_to_pa(&s_laboot_kva)),
                                               PhyAddr(kva_to_pa(&s_laboot_reclaimable_kva))));
        bootinfo_builder.append_reserved(PhyArea(PhyAddr(kva_to_pa(&s_laboot_reclaimable_kva)),
                                                 PhyAddr(kva_to_pa(&e_laboot_reclaimable_kva))),
                                         MemoryType::BOOT_RECLAIMABLE);
        bootinfo_builder.append_reserved(
            PhyArea(PhyAddr(kva_to_pa(&e_laboot_reclaimable_kva)), PhyAddr(kva_to_pa(&s_init))),
            MemoryType::RESERVED);
        bootinfo_builder.append_reserved(
            PhyArea(PhyAddr(kva_to_pa(&s_init)), PhyAddr(kva_to_pa(&e_init))),
            MemoryType::BOOT_RECLAIMABLE);
        bootinfo_builder.append_reserved(
            PhyArea(PhyAddr(kva_to_pa(&e_init)), PhyAddr(kva_to_pa(&ekernel))),
            MemoryType::RESERVED);
        return bootinfo_builder.write(cursor, kva_to_pa(&e_laboot_reclaimable_kva),
                                      PAGE_TABLE_ALIGNMENT, PhyAddr(laboot_info->dtb_phys));
    }

    extern "C" _LABOOT_POST_TEXT [[noreturn]] void _laboot_post_start(addr_t boot_ptr,
                                                                      addr_t reclaimable_cursor) {
        serial_puts(POST_MSG);
        auto *laboot_info = reinterpret_cast<LabootInfo *>(boot_ptr);
        auto *bootinfo    = build_bootinfo(laboot_info, reclaimable_cursor);
        bootinfo->hart_id = static_cast<size_t>(laboot_info->bsp_phys_id);
        serial_puts(FDT_FOUND_MSG);
        serial_puts(KERNEL_ENTRY_MSG);
        __bsp_start(bootinfo->hart_id, bootinfo);
    }
}  // namespace laboot
