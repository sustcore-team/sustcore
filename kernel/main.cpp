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
    Serial::serial_write_string(str);
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
DECLARE_LOGGER(KernelIO, LogLevel::DEBUG, KernelSetup)

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
    kprintf("我是post_init函数!\n");
    while (true);
}

void pre_init(void) {
    Initialization::pre_init();

    memset(regions, 0, sizeof(regions));
    int cnt = MemLayout::detect_memory_layout(regions, 128);
    void *upper_bound = nullptr;
    for (int i = 0; i < cnt; i++) {
        kprintf("探测到内存区域 %d: [%p, %p) Status: %d\n", i, regions[i].ptr,
                (void*)((umb_t)(regions[i].ptr) + regions[i].size),
                static_cast<int>(regions[i].status));
        void *this_bound = (void *)((umb_t)(regions[i].ptr) + regions[i].size);
        if (upper_bound < this_bound) {
            upper_bound = this_bound;
        }
    }

    kprintf("初始化线性增长PFA\n");
    PFA::pre_init(regions, cnt);

    kprintf("初始化内核地址空间管理器\n");
    PageMan::pre_init();
    kernel_paging_setup(upper_bound);

    typedef void (*PostTestFuncType)(void);
    PostTestFuncType post_test_func = (PostTestFuncType)PA2KA((void *)post_init);
    kprintf("跳转到内核虚拟地址空间中的post_init函数: %p\n", post_test_func);
    post_test_func();
}

void kernel_setup(void) {
    pre_init();
    while (true);
}
