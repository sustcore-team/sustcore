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

#include <cstddef>
#include <cassert>

extern "C"
void _init(void);
extern "C"
void _fini(void);

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
        for (size_t i = 0 ; i < count2; i++) {
            __fini_array_start[i]();
        }
    }
}

extern "C" void _cpp_setup(void) {
    while (true);
}