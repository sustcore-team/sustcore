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

#include <arch/trait.h>
#include <basecpp/baseio.h>
#include <configuration.h>
#include <basecpp/logger.h>
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

int kputs(const char* str) {
    if (! post_init_flag) {
        Serial::serial_write_string(str);
    }
    else {
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

void init_ker_paddr(void *upper_bound);
void mapping_kernel_areas(PageMan &man);

void kernel_paging_setup(void *upper_bound) {
    // 创建内核页表管理器
    kernel_root = PageMan::make_root();
    PageMan kernelman(kernel_root);

    init_ker_paddr(upper_bound);
    mapping_kernel_areas(kernelman);

    // 对[0, upper_bound)进行恒等映射
    size_t sz = (size_t)upper_bound;
    kernelman.map_range<true>(
        (void *)0, (void *)0, sz, PageMan::rwx(true, true, true), false, true
    );

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

    // 架构后初始化
    Initialization::post_init();

    while (true);
}

void pre_init(void) {
    Initialization::pre_init();

    memset(regions, 0, sizeof(regions));
    int cnt = MemLayout::detect_memory_layout(regions, 128);
    void *upper_bound = nullptr;
    for (int i = 0; i < cnt; i++) {
        LOGGER.INFO("探测到内存区域 %d: [%p, %p) Status: %d", i, regions[i].ptr,
                (void*)((umb_t)(regions[i].ptr) + regions[i].size),
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
    kernel_paging_setup(upper_bound);

    // 进入 post-init 阶段
    // 此阶段内, 内核的所有代码和数据均已映射到内核虚拟地址空间
    typedef void (*PostTestFuncType)(void);
    PostTestFuncType post_test_func = (PostTestFuncType)PA2KA((void *)post_init);
    LOGGER.DEBUG("跳转到内核虚拟地址空间中的post_init函数: %p", post_test_func);
    post_test_func();
}

void kernel_setup(void) {
    pre_init();
    while (true);
}
