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

#include <prm.h>
#include <sustcore/startup.h>
#include <sustcore/capability.h>
#include <kmod/syscall.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

typedef void (*_init_func)(void);

extern _init_func __init_array_start[0], __init_array_end[0];

size_t __heap_base;
size_t __current_brk;
CapIdx __pcb_cap;
CapIdx __main_tcb_cap;
CapIdx __heap_mem_cap;
CapIdx __stack_mem_cap;
void *__startup_data;
size_t __startup_size;

namespace kmod {
    void init(void) {
        const size_t count1 = __init_array_end - __init_array_start;

        // 执行init
        for (size_t i = 0; i < count1; i++) {
            __init_array_start[i]();
        }
    }
}  // namespace kmod

void kmod_main(void);

extern "C" void _cpp_setup(const void *stack_start) {
    if (stack_start == nullptr) {
        while (true) {}
    }

    const auto *base = static_cast<const char *>(stack_start);
    size_t total_size = 0;
    memcpy(&total_size, base, sizeof(total_size));
    const auto *startup = reinterpret_cast<const task::StartupInfo *>(
        base + sizeof(size_t));
    __startup_size =
        total_size > sizeof(size_t) + sizeof(task::StartupInfo)
            ? total_size - sizeof(size_t) - sizeof(task::StartupInfo)
            : 0;
    __startup_data = __startup_size == 0
                         ? nullptr
                         : const_cast<char *>(base + sizeof(size_t) +
                                              sizeof(task::StartupInfo));

    __heap_base     = startup->heap_vaddr.arith();
    __current_brk   = startup->heap_vaddr.arith();
    __pcb_cap       = startup->pcb_cap;
    __main_tcb_cap  = startup->main_tcb_cap;
    __heap_mem_cap  = startup->heap_mem_cap;
    __stack_mem_cap = startup->stack_mem_cap;

    kmod::init();

    kmod_main();
    while (true);
}
