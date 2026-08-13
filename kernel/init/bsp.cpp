/**
 * @file bsp.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内存、全局堆与布局系统的 BSP bring-up。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <init/kinit.h>
#include <init/milestones.h>
#include <log.h>
#include <memory/physical/buddy.h>
#include <memory/physical/gfp.h>
#include <memory/physical/page_database.h>
#include <memory/reclaim.h>
#include <memory/virtual/client/client_space.h>
#include <memory/virtual/kernel/kernel_mm.h>
#include <memory/virtual/kernel/kernel_space.h>
#include <sustcore/addrspace.h>
#include <tay/unique_ptr.h>
#ifdef CONFIG_KERNEL_SELFTEST
#include <test/cap.h>
#endif

#include <new>

namespace {
#ifndef NDEBUG
    void heap_smoke_test() noexcept {
        auto *small = new (std::nothrow) u64_t{0x53555354434f5245ULL};
        if (small == nullptr || *small != 0x53555354434f5245ULL)
            kernel::log::panic("内核堆小对象测试失败");
        delete small;

        auto *large = new (std::nothrow) std::byte[64 * 1024];
        if (large == nullptr)
            kernel::log::panic("内核堆大对象测试失败");
        large[0]             = std::byte{0x5a};
        large[64 * 1024 - 1] = std::byte{0xa5};
        delete[] large;
        kernel::log::info("内核堆分配 ABI 冒烟测试通过");
    }

    void layout_smoke_test() noexcept {
        constexpr u64_t TEST_OWNER = 0x54455354;
        auto allocation            = memory::gfp(1, memory::PageKind::RESERVED, TEST_OWNER);
        if (!allocation)
            kernel::log::panic("布局测试无法分配物理页");

        auto client = tay::create_unique<memory::ClientSpace, tay::error_code>();
        if (!client)
            kernel::log::panic("ClientSpace 创建测试失败: {}", static_cast<int>(client.error()));

        constexpr addr_t TEST_VADDR = KVA_START + 0x80000000ULL;
        auto loaded = memory::kernel_mm().load_kernel_layout(memory::KernelLayoutSpec{
            .virtual_base  = KvaAddr(TEST_VADDR),
            .physical_base = allocation->base(),
            .bytes         = PAGE_SIZE,
            .flags = memory::PageFlags{.readable = true, .writable = true, .executable = false},
        });
        if (!loaded)
            kernel::log::panic("KernelMM 布局加载测试失败: {}", static_cast<int>(loaded.error()));
        auto mapping = memory::kernel_space().query(HvaAddr(TEST_VADDR));
        if (!mapping || mapping->physical != allocation->base() || !mapping->flags.writable)
            kernel::log::panic("KernelMM 布局查询测试失败");

        (*client)->activate();
        auto *probe = reinterpret_cast<volatile u64_t *>(TEST_VADDR);
        *probe      = 0x4849474848414c46ULL;
        if (*probe != 0x4849474848414c46ULL)
            kernel::log::panic("ClientSpace 高半区绑定测试失败");
        memory::activate_kernel_space();

        auto unloaded = memory::kernel_mm().unload_kernel_layout(*loaded);
        if (!unloaded || memory::kernel_space().query(HvaAddr(TEST_VADDR)))
            kernel::log::panic("KernelMM 布局卸载测试失败");

        allocation->release();
        kernel::log::info("KernelMM 布局与 ClientSpace 根绑定冒烟测试通过");
    }
#endif
}  // namespace

extern "C" [[noreturn]] void bsp_main() {
    hal::install_runtime_exception_vectors();

#ifndef NDEBUG
    heap_smoke_test();
    layout_smoke_test();
#endif
#ifdef CONFIG_KERNEL_SELFTEST
    test::run_capability_selftest();
#endif

    const size_t init_reclaimed = memory::reclaim_init_memory();
    kernel::log::info("已释放 init 内存: {} 页", init_reclaimed);
    kernel::log::info("KernelMM、KernelSpace 与全局内核堆已就绪");
    init::run_kinit();
}
