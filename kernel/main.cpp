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

#include <arch/riscv64/trait.h>
#include <basecpp/baseio.h>
#include <kio.h>
#include <mem/alloc.h>
#include <sus/bits.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

void post_init(void) {}

void init(void) {}

int kputs(const char* str) {
    ArchSerial::serial_write_string(str);
    return strlen(str);
}

int kputchar(char ch) {
    ArchSerial::serial_write_char(ch);
    return ch;
}

char kgetchar() {
    return '\0';
}

KernelIO kio;

int KernelIO::putchar(char c) {
    return kputchar(c);
}

int KernelIO::puts(const char* str) {
    return kputs(str);
}

char KernelIO::getchar() {
    return kgetchar();
}

int kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vbprintf(kio, fmt, args);
    va_end(args);
    return len;
}

void kernel_setup(void) {
    ArchSerial::serial_write_string("欢迎使用 Sustcore Riscv64 内核!\n");
    ArchInitialization::pre_init();

    // FDTHelper::print_device_tree_detailed();

    MemRegion regions[128];
    int cnt = ArchMemoryLayout::detect_memory_layout(regions, 128);
    for (int i = 0; i < cnt; i++) {
        kprintf("Region %d: [%p, %p) Status: %d\n", i, regions[i].ptr,
                (void*)((umb_t)(regions[i].ptr) + regions[i].size),
                static_cast<int>(regions[i].status));
    }
    while (true);
}