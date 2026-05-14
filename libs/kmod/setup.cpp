/**
 * @file setup.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief setup function
 * @version alpha-1.0.0
 * @date 2026-02-23
 *
 * @copyright Copyright (c) 2026
 *
 */

// cpp setup入口点

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" void _init(void);
extern "C" void _fini(void);

typedef void (*_init_func)(void);
typedef void (*_fini_func)(void);

extern _init_func __init_array_start[0], __init_array_end[0];
extern _fini_func __fini_array_start[0], __fini_array_end[0];

namespace kmod {
    void init(void) {
        const size_t count1 = __init_array_end - __init_array_start;
        const size_t count2 = __fini_array_end - __fini_array_start;

        _init();
        // 执行init
        for (size_t i = 0; i < count1; i++) {
            __init_array_start[i]();
        }

        // 执行fini
        _fini();
        for (size_t i = 0; i < count2; i++) {
            __fini_array_start[i]();
        }
    }
}  // namespace kmod

void kmod_main(void);

extern "C" {
void kwrites(const char *str, size_t len);
size_t sys_grow_vma(size_t heap_base, size_t newbrk);

static size_t heap_base;
static size_t current_brk;

int kputs(const char *str) {
    size_t len = strlen(str);
    kwrites(str, len);
    return len;
}

size_t brk(size_t newbrk) {
    if (newbrk == 0) {
        return current_brk;
    }

    size_t actual_brk = sys_grow_vma(heap_base, newbrk);
    current_brk       = actual_brk;
    return current_brk;
}

void *sbrk(ptrdiff_t increment) {
    size_t old_brk = current_brk;
    size_t newbrk  = old_brk;

    if (increment >= 0) {
        size_t inc = static_cast<size_t>(increment);
        if (SIZE_MAX - old_brk < inc) {
            return reinterpret_cast<void *>(-1);
        }
        newbrk = old_brk + inc;
    } else {
        size_t dec = size_t(0) - static_cast<size_t>(increment);
        if (old_brk < dec) {
            return reinterpret_cast<void *>(-1);
        }
        newbrk = old_brk - dec;
    }

    size_t actual_brk = brk(newbrk);
    if (actual_brk != newbrk) {
        return reinterpret_cast<void *>(-1);
    }
    return reinterpret_cast<void *>(old_brk);
}

void _cpp_setup(size_t heap_vaddr) {
    heap_base   = heap_vaddr;
    current_brk = heap_vaddr;

    kmod::init();
    kmod_main();
    while (true);
}
}
