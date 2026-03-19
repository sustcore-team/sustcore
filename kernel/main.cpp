/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核Main函数
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/description.h>
#include <arch/riscv64/mem/sv39.h>
#include <arch/trait.h>
#include <cap/capability.h>
#include <cap/cholder.h>
#include <cap/cspace.h>
#include <device/block.h>
#include <fs/tarfs.h>
#include <kio.h>
#include <mem/addr.h>
#include <mem/alloc.h>
#include <mem/gfp.h>
#include <mem/kaddr.h>
#include <mem/slub.h>
#include <object/csa.h>
#include <perm/csa.h>
#include <perm/permission.h>
#include <sus/baseio.h>
#include <sus/defer.h>
#include <sus/logger.h>
#include <sus/path.h>
#include <sus/raii.h>
#include <sus/tree.h>
#include <sus/types.h>
#include <symbols.h>
#include <task/task.h>
#include <test/framework.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string_view>

#include <unordered_map>

int kputs(const char *str) {
    size_t len        = strlen(str);
    PhyAddr str_paddr = convert_pointer(str);
    Serial::serial_write_string(len, str_paddr.as<char>());
    return strlen(str);
}

int kputchar(char ch) {
    Serial::serial_write_char(ch);
    return ch;
}

char kgetchar() {
    return '\0';
}

int KernelIO::putchar(char c) {
    return kputchar(c);
}

int KernelIO::puts(const char *str) {
    return kputs(str);
}

char KernelIO::getchar() {
    return kgetchar();
}

int kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vbprintf<KernelIO>(fmt, args);
    va_end(args);
    return len;
}

int kprintfln(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vbprintf<KernelIO>(fmt, args);
    va_end(args);
    kputchar('\n');
    return len + 1;
}

#ifndef __SUS_NO_RTTI__
#error \
    "RTTI is not supported in the kernel. Please define __SUS_NO_RTTI__ to compile."
#endif

#ifndef __SUS_NO_EXCEPTIONS__
#error \
    "Exceptions are not supported in the kernel. Please define __SUS_NO_EXCEPTIONS__ to compile."
#endif

/**
 * @brief This function will just print the exception
 * and then halt the system. It will never return.
 * @param s the exception to throw
 */
[[noreturn]]
void __sus_cxa_throw(const std::exception &e) {
    kprintfln(
        ANSI_GRAPHIC(
            ANSI_FG_RED) "There is an exception of type %s: %s" ANSI_GRAPHIC(ANSI_GM_RESET),
        e.type(), e.what());
    while (true);
}

RamDiskDevice *make_initrd(void) {
    size_t sz             = (char *)&e_initrd - (char *)&s_initrd;
    RamDiskDevice *device = new RamDiskDevice(&s_initrd, sz, 1);
    LOGGER::INFO("initrd大小为 %u KB", sz / 1024, sz / 1024 / 1024);
    return device;
}

MemRegion regions[128];
size_t region_cnt   = 0;
PhyAddr kernel_root = PhyAddr::null;
const PhyAddr lowpm = PhyAddr::null;
PhyAddr uppm        = PhyAddr::null;
const VirAddr lowvm = VirAddr::null;

void run_defers(void *s_defer, void *e_defer) {
    constexpr size_t DEFER_ENTRY_SIZE = sizeof(util::DeferEntry);
    size_t defer_seg_size             = (char *)e_defer - (char *)s_defer;
    assert(defer_seg_size % DEFER_ENTRY_SIZE == 0);
    size_t defer_count = defer_seg_size / DEFER_ENTRY_SIZE;
    LOGGER::INFO(
        "开始运行defer构造函数。本批次defer起始地址为%p, 终结于%p, "
        "总共%u个defer",
        s_defer, e_defer, defer_count);
    for (size_t i = 0; i < defer_count; i++) {
        util::DeferEntry *entry =
            (util::DeferEntry *)((uintptr_t)s_defer + i * DEFER_ENTRY_SIZE);
        bool duplicated = false;
        for (size_t j = 0; j < i; j++) {
            util::DeferEntry *prev =
                (util::DeferEntry *)((uintptr_t)s_defer + j * DEFER_ENTRY_SIZE);
            if (prev->_instance == entry->_instance &&
                prev->_constructor == entry->_constructor)
            {
                duplicated = true;
                LOGGER::WARN(
                    "跳过重复defer注册: 当前第%d项与第%d项重复, "
                    "defer实例地址为%p, 构造器为%p",
                    i, j, entry->_instance, entry->_constructor);
                break;
            }
        }
        if (duplicated) {
            continue;
        }
        LOGGER::DEBUG("运行第%d个defer构造函数, defer实例地址为%p, 构造器为%p",
                      i, entry->_instance, entry->_constructor);
        entry->_constructor(entry->_instance);
    }
}

void kernel_paging_setup(void) {
    [[maybe_unused]]
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    // 创建内核页表管理器
    kernel_root                 = EarlyPageMan::make_root();
    EarlyPageMan kernelman(kernel_root);

    ker_paddr::init(lowpm, uppm);
    ker_paddr::mapping_kernel_areas(kernelman);

    // 对[0, uppm)进行恒等映射
    size_t sz = uppm - lowpm;
    kernelman.map_range<true>(lowvm, lowpm, sz,
                              EarlyPageMan::rwx(true, true, true), false, true);

    kernelman.switch_root();
    kernelman.flush_tlb();
}

extern "C" void post_init(void) {
    // logger
    LOGGER::INFO("已进入 post-init 阶段");

    // 将 pre-init 阶段中初始化的子系统再次初始化, 以适应内核虚拟地址空间
    GFP::post_init(regions, region_cnt);
    PageMan::init();

    // 初始化默认 Allocator 子系统
    Allocator::init();

    // 收集全局对象Defer并执行构造
    run_defers(&s_defer, &e_defer);

    // 初始化中断
    Interrupt::init();

    // 架构后初始化
    Initialization::post_init();

    // 将低端内存设置为用户态
    PageMan kernelman(kernel_root);
    kernelman.modify_range_flags<PageMan::ModifyMask::U>(
        lowvm, uppm - lowpm, PageMan::RWX::NONE, true, false);

    TestFramework framework;
    collect_tests(framework);
    framework.run_all();

    LOGGER::INFO("Test complete. Entering idle loop.");

    while (true);
}

extern "C" void redive(void);

void pre_init(void) {
    [[maybe_unused]]
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    Initialization::pre_init();

    memset(regions, 0, sizeof(regions));
    region_cnt          = MemoryLayout::detect_memory_layout(regions, 128);
    PhyAddr upper_bound = PhyAddr::null;
    for (int i = 0; i < region_cnt; i++) {
        PhyAddr start = regions[i].ptr;
        PhyAddr end   = start + regions[i].size;

        LOGGER::INFO("探测到内存区域 %d: [%p, %p) Status: %d", i, start.addr(),
                     end.addr(), static_cast<int>(regions[i].status));
        if (upper_bound < end) {
            upper_bound = end;
        }
    }

    LOGGER::INFO("初始化GFP");
    GFP::pre_init(regions, region_cnt);

    LOGGER::INFO("初始化内核地址空间管理器");
    EarlyPageMan::init();
    uppm = upper_bound;
    kernel_paging_setup();

    // 进入 post-init 阶段
    // 此阶段内, 内核的所有代码和数据均已映射到内核虚拟地址空间
    typedef void (*RediveFuncType)(void);
    PhyAddr redive_paddr  = (PhyAddr)(void *)redive;
    KvaAddr redive_kvaddr = convert<KvaAddr>(redive_paddr);
    LOGGER::DEBUG("redive函数物理地址: %p, 内核虚拟地址: %p",
                  redive_paddr.addr(), redive_kvaddr.addr());
    RediveFuncType redive_func = (RediveFuncType)redive_kvaddr.addr();
    LOGGER::DEBUG("跳转到内核虚拟地址空间中的redive函数: %p", redive_func);
    redive_func();
    LOGGER::ERROR("redive函数返回了, 这不应该发生!");
    while (true);
}

void kernel_setup(void) {
    pre_init();
    while (true);
}