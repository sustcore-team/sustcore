/**
 * @file la_post_boot.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief laboot 第二阶段: 构造 BootInfo 并进入内核
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <boot/laboot/la_paging.h>
#include <libfdt.h>
#include <sustcore/boot.h>

#include <cstring>

#define _LABOOT_POST_FUNCTION  SECTION(".laboot_post.text")
#define _LABOOT_POST_DATA      SECTION(".laboot_post.data")
#define _LABOOT_POST_RODATA    SECTION(".laboot_post.rodata")
#define _LABOOT_POST_STRING(x) _LABOOT_POST_RODATA constexpr const char x[]

namespace laboot::post {
    _LABOOT_POST_DATA volatile uint8_t *SERIAL_BASE =
        reinterpret_cast<volatile uint8_t *>(0x1fe001e0ULL);

    _LABOOT_POST_FUNCTION void serial_putc(char ch) {
        while ((SERIAL_BASE[5] & 0x20) == 0) {
        }
        SERIAL_BASE[0] = static_cast<uint8_t>(ch);
    }

    _LABOOT_POST_FUNCTION void serial_puts(const char *str) {
        for (const char *p = str; *p != '\0'; ++p) {
            serial_putc(*p);
        }
    }
}  // namespace laboot::post

namespace laboot::msg::post {
    _LABOOT_POST_STRING(POST_MSG)         = "LABOOT启动第二阶段!\n";
    _LABOOT_POST_STRING(KERNEL_ENTRY_MSG) = "LABOOT进入内核入口!\n";
    _LABOOT_POST_STRING(BOOTINFO_OVERFLOW_MSG) =
        "错误: BootInfo 区域数量超限\n";
    _LABOOT_POST_STRING(BOOTINFO_ALLOC_MSG) =
        "错误: LABOOT reclaimable 区域不足\n";
    _LABOOT_POST_STRING(INVALID_DTB_MSG) = "错误: FDT 无效\n";
    _LABOOT_POST_STRING(BOOTINFO_TOO_LARGE_MSG) =
        "错误: BootInfo 超过 128KB 限制\n";
    _LABOOT_POST_STRING(FDT_FOUND_MSG) = "LABOOT 成功校验并处理启动数据\n";
}  // namespace laboot::msg::post

namespace laboot {
    using namespace post;
    using namespace msg::post;

    constexpr size_t MAX_BOOTINFO_REGIONS  = 128;
    constexpr size_t CELL_SIZE             = sizeof(fdt32_t);
    constexpr size_t EFI_MAX_CONFIG_TABLES = 4096;

    struct efi_guid_t {
        uint32_t data1;
        uint16_t data2;
        uint16_t data3;
        uint8_t data4[8];
    };

    struct efi_configuration_table_t {
        efi_guid_t vendor_guid;
        void *vendor_table;
    };

    struct efi_system_table_t {
        uint64_t hdr[3];
        void *firmware_vendor;
        uint32_t firmware_revision;
        void *console_in_handle;
        void *con_in;
        void *console_out_handle;
        void *con_out;
        void *standard_error_handle;
        void *std_err;
        void *runtime_services;
        void *boot_services;
        size_t number_of_table_entries;
        efi_configuration_table_t *configuration_table;
    };

    constexpr efi_guid_t EFI_DTB_TABLE_GUID{
        .data1=0xb1b621d5,
        .data2=0xf19c,
        .data3=0x41a5,
        .data4={0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0},
    };

    extern "C" void c_setup(size_t hart_id, BootInfoHeader *bootinfo);

    [[noreturn]] _LABOOT_POST_FUNCTION void post_panic(const char *msg) {
        serial_puts(msg);
        while (true) {
        }
    }

    _LABOOT_POST_FUNCTION addr_t kva_to_pa(const char *ptr) {
        return reinterpret_cast<addr_t>(ptr) - KVA_OFFSET;
    }

    _LABOOT_POST_FUNCTION addr_t align_down(addr_t value, addr_t align) {
        return value & ~(align - 1);
    }

    _LABOOT_POST_FUNCTION addr_t align_up(addr_t value, addr_t align) {
        return align_down(value + align - 1, align);
    }

    _LABOOT_POST_FUNCTION void *pa_to_hhdm(addr_t phys) {
        if (phys == 0) {
            return nullptr;
        }
        return reinterpret_cast<void *>(phys + KPA_OFFSET);
    }

    _LABOOT_POST_FUNCTION bool guid_equal(const efi_guid_t &lhs,
                                          const efi_guid_t &rhs) {
        if (lhs.data1 != rhs.data1 || lhs.data2 != rhs.data2 ||
            lhs.data3 != rhs.data3)
        {
            return false;
        }
        for (size_t i = 0; i < sizeof(lhs.data4); ++i) {
            if (lhs.data4[i] != rhs.data4[i]) {
                return false;
            }
        }
        return true;
    }

    _LABOOT_POST_FUNCTION efi_configuration_table_t *config_tables(
        LabootInfo &boot_info, size_t *count) {
        if (count != nullptr) {
            *count = 0;
        }

        auto *system_table = static_cast<efi_system_table_t *>(
            pa_to_hhdm(boot_info.system_table_phys));
        if (system_table == nullptr ||
            system_table->configuration_table == nullptr ||
            system_table->number_of_table_entries == 0 ||
            system_table->number_of_table_entries > EFI_MAX_CONFIG_TABLES)
        {
            return nullptr;
        }

        size_t table_count = system_table->number_of_table_entries;
        addr_t config_phys =
            reinterpret_cast<addr_t>(system_table->configuration_table);
        auto *tables =
            static_cast<efi_configuration_table_t *>(pa_to_hhdm(config_phys));
        if (tables == nullptr) {
            return nullptr;
        }

        boot_info.system_table_virt = reinterpret_cast<addr_t>(system_table);

        if (count != nullptr) {
            *count = table_count;
        }
        return tables;
    }

    _LABOOT_POST_FUNCTION void *find_config_table(LabootInfo &boot_info,
                                                  const efi_guid_t &guid) {
        size_t table_count = 0;
        auto *tables       = config_tables(boot_info, &table_count);
        if (tables == nullptr) {
            return nullptr;
        }

        for (size_t i = 0; i < table_count; ++i) {
            if (guid_equal(tables[i].vendor_guid, guid)) {
                return tables[i].vendor_table;
            }
        }
        return nullptr;
    }

    _LABOOT_POST_FUNCTION void find_dtb_from_system_table(
        LabootInfo &boot_info) {
        if (boot_info.dtb_virt != 0) {
            return;
        }

        auto *dtb_table = find_config_table(boot_info, EFI_DTB_TABLE_GUID);
        if (dtb_table == nullptr) {
            post_panic(INVALID_DTB_MSG);
        }

        boot_info.dtb_phys = reinterpret_cast<addr_t>(dtb_table);
        boot_info.dtb_virt =
            reinterpret_cast<addr_t>(pa_to_hhdm(boot_info.dtb_phys));
        if (boot_info.dtb_virt == 0 ||
            fdt_check_header(
                reinterpret_cast<const void *>(boot_info.dtb_virt)) != 0)
        {
            post_panic(INVALID_DTB_MSG);
        }
    }

    _LABOOT_POST_FUNCTION bool region_empty(const MemRegion &region) {
        return region.area.nullable();
    }

    _LABOOT_POST_FUNCTION addr_t region_end(const MemRegion &region) {
        return region.area.end.arith();
    }

    _LABOOT_POST_FUNCTION void append_region(MemRegion *regions, size_t &cnt,
                                             PhyArea area,
                                             MemRegion::MemoryStatus status) {
        if (area.nullable()) {
            return;
        }
        if (cnt >= MAX_BOOTINFO_REGIONS) {
            post_panic(BOOTINFO_OVERFLOW_MSG);
        }
        regions[cnt] = MemRegion{
            .status = status,
            .area   = area,
        };
        ++cnt;
    }

    _LABOOT_POST_FUNCTION bool region_less(const MemRegion &lhs,
                                           const MemRegion &rhs) {
        if (lhs.area.begin != rhs.area.begin) {
            return lhs.area.begin < rhs.area.begin;
        }
        if (lhs.area.end != rhs.area.end) {
            return lhs.area.end < rhs.area.end;
        }
        return static_cast<int>(lhs.status) < static_cast<int>(rhs.status);
    }

    _LABOOT_POST_FUNCTION void sort_regions(MemRegion *regions, size_t cnt) {
        for (size_t i = 1; i < cnt; ++i) {
            MemRegion key = regions[i];
            size_t j      = i;
            while (j > 0 && region_less(key, regions[j - 1])) {
                regions[j] = regions[j - 1];
                --j;
            }
            regions[j] = key;
        }
    }

    _LABOOT_POST_FUNCTION bool mergeable(const MemRegion &lhs,
                                         const MemRegion &rhs) {
        return lhs.status == rhs.status &&
               region_end(lhs) >= rhs.area.begin.arith();
    }

    _LABOOT_POST_FUNCTION size_t merge_regions(MemRegion *regions, size_t cnt) {
        if (cnt == 0) {
            return 0;
        }
        sort_regions(regions, cnt);
        size_t dst = 0;
        for (size_t i = 0; i < cnt; ++i) {
            if (region_empty(regions[i])) {
                continue;
            }
            if (dst > 0 && mergeable(regions[dst - 1], regions[i])) {
                addr_t end = region_end(regions[i]);
                if (region_end(regions[dst - 1]) < end) {
                    regions[dst - 1].area.end = PhyAddr(end);
                }
                continue;
            }
            regions[dst++] = regions[i];
        }
        return dst;
    }

    _LABOOT_POST_FUNCTION uint64_t read_be(const void *data, size_t cells) {
        const auto *value = static_cast<const fdt32_t *>(data);
        uint64_t result   = 0;
        for (size_t i = 0; i < cells; ++i) {
            result = (result << 32) | fdt32_to_cpu(value[i]);
        }
        return result;
    }

    _LABOOT_POST_FUNCTION int parent_cell_count(const void *dtb, int node,
                                                const char *name,
                                                int fallback) {
        int parent = fdt_parent_offset(dtb, node);
        if (parent < 0) {
            parent = 0;
        }
        int len = 0;
        const auto *prop =
            static_cast<const fdt32_t *>(fdt_getprop(dtb, parent, name, &len));
        if (prop == nullptr || len != static_cast<int>(sizeof(fdt32_t))) {
            return fallback;
        }
        return static_cast<int>(fdt32_to_cpu(*prop));
    }

    _LABOOT_POST_FUNCTION bool node_enabled(const void *dtb, int node) {
        int len = 0;
        const char *status =
            static_cast<const char *>(fdt_getprop(dtb, node, "status", &len));
        if (status == nullptr) {
            return true;
        }
        return strcmp(status, "okay") == 0 || strcmp(status, "ok") == 0;
    }

    _LABOOT_POST_FUNCTION void append_reg_regions(
        const void *dtb, int node, MemRegion *regions, size_t &cnt,
        MemRegion::MemoryStatus status) {
        int len            = 0;
        const void *reg    = fdt_getprop(dtb, node, "reg", &len);
        int addr_cells     = parent_cell_count(dtb, node, "#address-cells", 2);
        int size_cells     = parent_cell_count(dtb, node, "#size-cells", 2);
        int cells_per_item = addr_cells + size_cells;
        if (reg == nullptr || len <= 0 || cells_per_item <= 0 ||
            len % static_cast<int>(cells_per_item * CELL_SIZE) != 0)
        {
            return;
        }

        const char *cursor = static_cast<const char *>(reg);
        for (int offset  = 0; offset < len;
             offset     += cells_per_item * static_cast<int>(CELL_SIZE))
        {
            addr_t begin = static_cast<addr_t>(
                read_be(cursor + offset, static_cast<size_t>(addr_cells)));
            size_t size = static_cast<size_t>(
                read_be(cursor + offset + addr_cells * CELL_SIZE,
                        static_cast<size_t>(size_cells)));
            append_region(regions, cnt,
                          PhyArea(PhyAddr(begin), PhyAddr(begin + size)),
                          status);
        }
    }

    _LABOOT_POST_FUNCTION void collect_memory_regions(const void *dtb,
                                                      MemRegion *regions,
                                                      size_t &cnt) {
        int node = -1;
        while ((node = fdt_next_node(dtb, node, nullptr)) >= 0) {
            if (!node_enabled(dtb, node)) {
                continue;
            }
            int len                 = 0;
            const char *device_type = static_cast<const char *>(
                fdt_getprop(dtb, node, "device_type", &len));
            if (device_type == nullptr || strcmp(device_type, "memory") != 0) {
                continue;
            }
            append_reg_regions(dtb, node, regions, cnt,
                               MemRegion::MemoryStatus::FREE);
        }
    }

    _LABOOT_POST_FUNCTION void collect_reserved_regions(const void *dtb,
                                                        MemRegion *regions,
                                                        size_t &cnt) {
        int reserved = fdt_path_offset(dtb, "/reserved-memory");
        if (reserved < 0) {
            return;
        }

        int child = 0;
        fdt_for_each_subnode(child, dtb, reserved) {
            if (!node_enabled(dtb, child)) {
                continue;
            }
            append_reg_regions(dtb, child, regions, cnt,
                               MemRegion::MemoryStatus::RESERVED);
        }
    }

    _LABOOT_POST_FUNCTION void append_free_fragment(MemRegion *final_regions,
                                                    size_t &final_cnt,
                                                    addr_t begin, addr_t end) {
        if (begin < end) {
            append_region(final_regions, final_cnt,
                          PhyArea(PhyAddr(begin), PhyAddr(end)),
                          MemRegion::MemoryStatus::FREE);
        }
    }

    _LABOOT_POST_FUNCTION size_t
    normalize_boot_regions(MemRegion *memory_regions, size_t memory_cnt,
                           MemRegion *reserved_regions, size_t reserved_cnt,
                           MemRegion *final_regions) {
        memory_cnt   = merge_regions(memory_regions, memory_cnt);
        reserved_cnt = merge_regions(reserved_regions, reserved_cnt);

        size_t final_cnt = 0;
        for (size_t i = 0; i < reserved_cnt; ++i) {
            final_regions[final_cnt++] = reserved_regions[i];
        }

        for (size_t i = 0; i < memory_cnt; ++i) {
            addr_t current = memory_regions[i].area.begin.arith();
            addr_t end     = region_end(memory_regions[i]);

            for (size_t j = 0; j < reserved_cnt; ++j) {
                addr_t reserved_begin = reserved_regions[j].area.begin.arith();
                addr_t reserved_end   = region_end(reserved_regions[j]);
                if (reserved_end <= current) {
                    continue;
                }
                if (end <= reserved_begin) {
                    break;
                }
                append_free_fragment(final_regions, final_cnt, current,
                                     reserved_begin);
                if (current < reserved_end) {
                    current = reserved_end;
                }
                if (current >= end) {
                    break;
                }
            }
            append_free_fragment(final_regions, final_cnt, current, end);
        }

        return merge_regions(final_regions, final_cnt);
    }

    _LABOOT_POST_FUNCTION BootInfoHeader *build_bootinfo(
        LabootInfo *laboot_info, addr_t cursor) {
        MemRegion memory_regions[MAX_BOOTINFO_REGIONS];
        MemRegion reserved_regions[MAX_BOOTINFO_REGIONS];
        MemRegion final_regions[MAX_BOOTINFO_REGIONS];
        size_t memory_cnt   = 0;
        size_t reserved_cnt = 0;

        find_dtb_from_system_table(*laboot_info);

        const void *dtb = reinterpret_cast<const void *>(laboot_info->dtb_virt);
        if (dtb == nullptr || fdt_check_header(dtb) != 0) {
            post_panic(INVALID_DTB_MSG);
        }

        collect_memory_regions(dtb, memory_regions, memory_cnt);
        collect_reserved_regions(dtb, reserved_regions, reserved_cnt);
        size_t dtb_size = static_cast<size_t>(fdt_totalsize(dtb));
        append_region(
            reserved_regions, reserved_cnt,
            PhyArea(PhyAddr(laboot_info->dtb_phys),
                    PhyAddr(laboot_info->dtb_phys + dtb_size)),
            MemRegion::MemoryStatus::BOOT_RECLAIMABLE);

        append_region(
            memory_regions, memory_cnt,
            PhyArea(PhyAddr(kva_to_pa(&s_laboot_kva)),
                    PhyAddr(kva_to_pa(&s_laboot_reclaimable_kva))),
            MemRegion::MemoryStatus::FREE);
        append_region(reserved_regions, reserved_cnt,
                      PhyArea(PhyAddr(kva_to_pa(&s_laboot_reclaimable_kva)),
                              PhyAddr(kva_to_pa(&e_laboot_reclaimable_kva))),
                      MemRegion::MemoryStatus::BOOT_RECLAIMABLE);
        append_region(
            reserved_regions, reserved_cnt,
            PhyArea(PhyAddr(kva_to_pa(&e_laboot_reclaimable_kva)),
                    PhyAddr(kva_to_pa(&ekernel))),
            MemRegion::MemoryStatus::RESERVED);

        size_t final_cnt =
            normalize_boot_regions(memory_regions, memory_cnt, reserved_regions,
                                   reserved_cnt, final_regions);

        addr_t aligned_cursor =
            (cursor + PAGE_TABLE_ALIGNMENT - 1) & ~(PAGE_TABLE_ALIGNMENT - 1);
        size_t info_size =
            sizeof(BootInfoHeader) + sizeof(MemRegion) * final_cnt +
            sizeof(PhyAddr);
        if (info_size > MAX_BOOTINFO_SIZE) {
            post_panic(BOOTINFO_TOO_LARGE_MSG);
        }
        if (aligned_cursor + info_size > kva_to_pa(&e_laboot_reclaimable_kva)) {
            post_panic(BOOTINFO_ALLOC_MSG);
        }

        auto *header       = reinterpret_cast<BootInfoHeader *>(aligned_cursor);
        header->info_sz    = info_size;
        header->region_cnt = final_cnt;
        auto *regions      = bootinfo_regions(header);
        for (size_t i = 0; i < final_cnt; ++i) {
            regions[i] = final_regions[i];
        }
        *bootinfo_fdt_pa(header) = PhyAddr(laboot_info->dtb_phys);
        return header;
    }

    extern "C" _LABOOT_POST_FUNCTION [[noreturn]]
    void _laboot_post_start(addr_t boot_ptr, addr_t reclaimable_cursor) {
        serial_puts(POST_MSG);
        auto *bootinfo = build_bootinfo(
            reinterpret_cast<LabootInfo *>(boot_ptr), reclaimable_cursor);
        bootinfo->hart_id = static_cast<size_t>(
            reinterpret_cast<LabootInfo *>(boot_ptr)->bsp_phys_id);
        serial_puts(FDT_FOUND_MSG);
        serial_puts(KERNEL_ENTRY_MSG);
        c_setup(bootinfo->hart_id, bootinfo);
        while (true) {
        }
    }
}  // namespace laboot
