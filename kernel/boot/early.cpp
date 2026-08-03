/**
 * @file early.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief BSP 早期启动流程
 * @version 0.1.0-dev.1
 * @date 2026-08-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/interrupt.h>
#include <boot/common/bytes.h>
#include <boot/context.h>
#include <boot/early.h>
#include <boot/early_internal.h>
#include <boot/sections.h>
#include <init/milestones.h>
#include <log.h>
#include <memory/physical/buddy.h>
#include <memory/physical/page_database.h>
#include <memory/slab/heap.h>
#include <memory/virtual/kernel/kernel_mm.h>
#include <memory/virtual/kernel/kernel_space.h>
#include <sustcore/addrspace.h>
#include <tay/bits.h>
#include <tay/static_vector.h>

#include <cstddef>

extern "C" [[noreturn]] void bsp_main();

using area_vector = tay::static_vector<PhyArea, MAX_BOOTINFO_REGIONS>;

alignas(PAGE_SIZE) BOOT_INIT_BSS constinit byte bootinfo_storage[MAX_BOOTINFO_SIZE];
BOOT_INIT_BSS constinit const BootInfoHeader *saved_bootinfo = nullptr;
BOOT_INIT_BSS constinit size_t saved_boot_hwid               = 0;

BOOT_INIT_TEXT void copy_bootinfo(const BootInfoHeader *source) noexcept {
    __early_copy(bootinfo_storage, source, source->info_sz);
    // 对齐的字节存储保存完整尾随布局，此后可按 BootInfoHeader 只读访问。
    saved_bootinfo =
        reinterpret_cast<const BootInfoHeader *>(static_cast<const void *>(bootinfo_storage));
}

BOOT_INIT_TEXT void append_area(area_vector &areas, PhyArea area) noexcept {
    if (area.nullable())
        return;
    if (!areas.empty() && areas.back().end == area.begin) {
        areas.back().end = area.end;
        return;
    }
    if (!areas.push_back(area))
        kernel::log::panic("可用区域 static_vector 容量已耗尽");
}

BOOT_INIT_TEXT void sort_areas(area_vector &areas) noexcept {
    for (size_t index = 1; index < areas.size(); ++index) {
        PhyArea key   = areas[index];
        size_t cursor = index;
        while (cursor != 0 && key.begin < areas[cursor - 1].begin) {
            areas[cursor] = areas[cursor - 1];
            --cursor;
        }
        areas[cursor] = key;
    }
}

namespace boot {
    const BootInfoHeader *early_bootinfo() noexcept {
        return saved_bootinfo;
    }

    size_t early_boot_hwid() noexcept {
        return saved_boot_hwid;
    }

    BOOT_INIT_TEXT void publish_usable_areas(const BootInfoHeader &bootinfo) noexcept {
        area_vector usable;
        area_vector exclusions;
        const auto *regions  = bootinfo_regions(&bootinfo);
        const auto &database = memory::page_database();

        for (size_t parent_idx = 0; parent_idx < database.region_count(); ++parent_idx) {
            exclusions.clear();
            const auto &physical_region = database.region(parent_idx);
            if (!exclusions.push_back(physical_region.metadata_backing))
                kernel::log::panic("保留区域 static_vector 容量已耗尽");

            size_t child_idx = regions[parent_idx].rsvd_idx;
            while (child_idx != parent_idx) {
                if (!exclusions.push_back(regions[child_idx].area))
                    kernel::log::panic("保留区域 static_vector 容量已耗尽");
                const size_t next = regions[child_idx].rsvd_idx;
                if (next == child_idx)
                    break;
                child_idx = next;
            }
            sort_areas(exclusions);

            addr_t cursor = physical_region.parent.begin.arith();
            for (const auto &excluded : exclusions) {
                if (cursor < excluded.begin.arith())
                    append_area(usable, PhyArea(PhyAddr(cursor), excluded.begin));
                if (cursor < excluded.end.arith())
                    cursor = excluded.end.arith();
            }
            if (cursor < physical_region.parent.end.arith())
                append_area(usable, PhyArea(PhyAddr(cursor), physical_region.parent.end));
        }

        // PageAllocation 目前以 PA 0 表示空值；在上层显式排除该页。
        if (!usable.empty() && usable.front().begin.arith() == 0) {
            if (usable.front().size() == PAGE_SIZE) {
                static_cast<void>(usable.erase(usable.begin()));
            } else {
                usable.front().begin = PhyAddr(PAGE_SIZE);
            }
        }

        size_t total_pages = 0;
        auto allocator     = memory::buddy();
        for (const auto &area : usable) {
            allocator->put_range(area);
            total_pages += area.size() / PAGE_SIZE;
        }
        kernel::log::info("已向 Buddy 发布 {} 个可用区域 ({} 页)", usable.size(), total_pages);
    }
}  // namespace boot

extern "C" [[noreturn]] BOOT_INIT_TEXT void __bsp_early_main(
    size_t bsp_hwid, const BootInfoHeader *source_bootinfo) {
    // 阶段一：建立最小 C++ 运行环境，此前不得依赖全局构造或普通异常处理。
    boot::early_internal::clear_bss();
    kernel::log::info("进入高半区, BSS 已清零");
    hal::disable_interrupts();
    hal::install_early_exception_vectors();
    kernel::log::info("异常向量已安装");

    init::advance(init::Milestone::RESET, init::Milestone::EARLT_CPPRT);
    kernel::log::info("早期 C++ 常量初始化运行时已就绪");

    // 阶段二：校验并复制启动器数据
    boot::early_internal::validate_bootinfo(bsp_hwid, source_bootinfo);
    copy_bootinfo(source_bootinfo);
    saved_boot_hwid = bsp_hwid;
    kernel::log::info("已校验并复制 BootInfo");

    memory::page_database().initialize(*saved_bootinfo);
    kernel::log::info("页数据库已初始化: {} 个物理区域", memory::page_database().region_count());
    memory::buddy()->initialize();
    boot::publish_usable_areas(*saved_bootinfo);
    {
        auto allocator = memory::buddy();
        auto probe     = allocator->try_gfp_in_order(0);
        if (!probe) {
            kernel::log::panic("探测 Buddy order-0 分配失败: {}", probe.error());
        }
        allocator->put_pages(*probe);
    }
    kernel::log::info("Buddy order-0 分配探测成功");
    kernel::log::info("Buddy 已就绪: {} 个空闲页", memory::buddy()->free_pages());
    init::advance(init::Milestone::EARLT_CPPRT, init::Milestone::MEMORY_READY);

    auto heap = memory::init_heap();
    if (!heap)
        kernel::log::panic("无法启用内核堆: {}", static_cast<int>(heap.error()));
    init::advance(init::Milestone::MEMORY_READY, init::Milestone::HEAP_READY);
    kernel::log::info("全局 SLUB 内核堆已就绪");

    boot::early_internal::run_initializers();
    init::advance(init::Milestone::HEAP_READY, init::Milestone::GLOBAL_CTORS_READY);
    kernel::log::info("C++ 全局构造函数已运行");

    boot::preserve(*saved_bootinfo);
    kernel::log::info("已持久化 BootInfo");
    memory::init_kernel_space(*boot::context().info);
    auto kernel_mm = memory::KernelMM::initialize(*boot::context().info);
    if (!kernel_mm)
        kernel::log::panic("无法初始化 KernelMM: {}", static_cast<int>(kernel_mm.error()));
    for (size_t index = 0; index < memory::page_database().region_count(); ++index) {
        const auto &area = memory::page_database().region(index).parent;
        if (!memory::kernel_mm().hhdm_covers(area.begin, area.size()))
            kernel::log::panic("最终 HHDM 未覆盖物理区域 {}", index);
    }
    memory::kernel_space().activate();
    init::advance(init::Milestone::GLOBAL_CTORS_READY, init::Milestone::VIRTUAL_MEMORY_READY);
    const size_t boot_reclaimed = boot::reclaim_boot_memory();
    kernel::log::info("已释放可回收启动内存: 共 {} 页", boot_reclaimed);

    bsp_main();
}
