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

#include <arch/riscv64/mem/sv39.h>
#include <arch/trait.h>
#include <basecpp/baseio.h>
#include <basecpp/logger.h>
#include <configuration.h>
#include <kio.h>
#include <mem/alloc.h>
#include <mem/kaddr.h>
#include <mem/pfa.h>
#include <sus/types.h>
#include <symbols.h>

#include <cstdarg>
#include <cstddef>
#include <cstring>

bool post_init_flag = false;

void buddy_test_complex();

int kputs(const char *str) {
    if (!post_init_flag) {
        Serial::serial_write_string(str);
    } else {
        const char *_str = (const char *)KA2PA((void *)str);
        Serial::serial_write_string(_str);
    }
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

MemRegion regions[128];
PageMan::PTE *kernel_root = nullptr;
void *phymem_upper_bound;

void init_ker_paddr(void *phymem_upper_bound);
void mapping_kernel_areas(PageMan &man);

void kernel_paging_setup() {
    // 创建内核页表管理器
    kernel_root = PageMan::make_root();
    PageMan kernelman(kernel_root);

    init_ker_paddr(phymem_upper_bound);
    mapping_kernel_areas(kernelman);

    // 对[0, phymem_upper_bound)进行恒等映射
    size_t sz = (size_t)phymem_upper_bound;
    kernelman.map_range<true>((void *)0, (void *)0, sz,
                              PageMan::rwx(true, true, true), false, true);

    kernelman.switch_root();
    kernelman.flush_tlb();
}

void post_init(void) {
    // 将 sp 移动到高位内存
    RELOAD_SP();

    // 设置post_init标志
    post_init_flag = true;

    // logger
    LOGGER.INFO("已进入 post-init 阶段");

    // 将 pre-init 阶段中初始化的子系统再次初始化, 以适应内核虚拟地址空间
    PFA::post_init();
    PageMan::post_init();

    // 初始化默认 Allocator 子系统
    Allocator::init();

    // 初始化中断
    ArchInterrupt::init();

    // 架构后初始化
    Initialization::post_init();

    buddy_test_complex();

    // 将低端内存设置为用户态
    PageMan kernelman(kernel_root);
    kernelman.modify_range_flags<PageMan::ModifyMask::U>(
        (void *)0, (size_t)phymem_upper_bound, PageMan::RWX::NONE, true, false);

    while (true);
}

void pre_init(void) {
    Initialization::pre_init();

    memset(regions, 0, sizeof(regions));
    int cnt           = MemLayout::detect_memory_layout(regions, 128);
    void *upper_bound = nullptr;
    for (int i = 0; i < cnt; i++) {
        LOGGER.INFO("探测到内存区域 %d: [%p, %p) Status: %d", i, regions[i].ptr,
                    (void *)((umb_t)(regions[i].ptr) + regions[i].size),
                    static_cast<int>(regions[i].status));
        void *this_bound = (void *)((umb_t)(regions[i].ptr) + regions[i].size);
        if (upper_bound < this_bound) {
            upper_bound = this_bound;
        }
    }

    LOGGER.INFO("初始化线性增长PFA");
    PFA::pre_init(regions, cnt);

    LOGGER.INFO("初始化内核地址空间管理器");
    PageMan::pre_init();
    phymem_upper_bound = upper_bound;
    kernel_paging_setup();

    // 进入 post-init 阶段
    // 此阶段内, 内核的所有代码和数据均已映射到内核虚拟地址空间
    typedef void (*PostTestFuncType)(void);
    PostTestFuncType post_test_func =
        (PostTestFuncType)PA2KA((void *)post_init);
    LOGGER.DEBUG("跳转到内核虚拟地址空间中的post_init函数: %p", post_test_func);
    post_test_func();
}

void kernel_setup(void) {
    pre_init();
    while (true);
}

void buddy_test_complex() {
    LOGGER.INFO("========== Complex Buddy Allocator Test Start ==========");

    // Scenario 1: Mixed Size Allocation & Fragmentation
    LOGGER.INFO("Scenario 1: Mixed Size & Fragmentation");
    void *p1 = PFA::alloc_frame(1);  // 1 page
    void *p2 = PFA::alloc_frame(2);  // 2 pages
    void *p4 = PFA::alloc_frame(4);  // 4 pages
    void *p3 = PFA::alloc_frame(3);  // 3 pages (odd size)

    if (p1 && p2 && p3 && p4) {
        LOGGER.INFO("Mixed Alloc Success: 1p@%p, 2p@%p, 4p@%p, 3p@%p", p1, p2,
                    p4, p3);

        // Free middle blocks to create fragmentation
        PFA::free_frame(p2, 2);
        PFA::free_frame(p3, 3);
        LOGGER.INFO("Freed middle blocks (2p, 3p)");

        // Try to alloc 2 pages again (should reuse p2 or part of p3)
        void *p_new = PFA::alloc_frame(2);
        LOGGER.INFO("Re-alloc 2 pages: %p", p_new);

        if (p_new)
            PFA::free_frame(p_new, 2);
        PFA::free_frame(p1, 1);
        PFA::free_frame(p4, 4);
    } else {
        LOGGER.ERROR("Mixed Alloc Failed");
        if (p1)
            PFA::free_frame(p1, 1);
        if (p2)
            PFA::free_frame(p2, 2);
        if (p4)
            PFA::free_frame(p4, 4);
        if (p3)
            PFA::free_frame(p3, 3);
    }

    // Scenario 2: Exhaustion Test (Specific Order)
    LOGGER.INFO("Scenario 2: Try to exhaust order 0 (1 page)");
    constexpr int MAX_singles = 32;
    void *singles[MAX_singles];
    int alloc_count = 0;

    for (int i = 0; i < MAX_singles; i++) {
        singles[i] = PFA::alloc_frame(1);
        if (!singles[i]) {
            LOGGER.WARN("Stopped at %d allocations", i);
            break;
        }
        alloc_count++;
    }
    LOGGER.INFO("Allocated %d single pages", alloc_count);

    // Reverse free to encourage merging
    for (int i = alloc_count - 1; i >= 0; i--) {
        PFA::free_frame(singles[i], 1);
    }
    LOGGER.INFO("Freed all single pages");

    // Scenario 3: Large Block Split & Merge
    LOGGER.INFO("Scenario 3: Large Block Split & Merge");
    void *huge = PFA::alloc_frame(64);  // Request 64 pages
    if (huge) {
        LOGGER.INFO("Huge block allocated at %p", huge);
        PFA::free_frame(huge, 64);
        LOGGER.INFO("Huge block freed");

        // Verify we can alloc it again (merge successful)
        void *huge2 = PFA::alloc_frame(64);
        if (huge2 == huge) {
            LOGGER.INFO(
                "Merge Verification Success: Same address re-allocated");
        } else {
            LOGGER.WARN(
                "Merge Verification: Got different address %p (Original: %p)",
                huge2, huge);
        }
        if (huge2)
            PFA::free_frame(huge2, 64);
    } else {
        LOGGER.ERROR("Huge block alloc failed");
    }

    LOGGER.INFO("========== Complex Buddy Allocator Test End ==========");
}