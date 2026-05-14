/**
 * @file main.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 主文件
 * @version alpha-1.0.0
 * @date 2026-02-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cstddef>
#include <cstdio>
#include <sus/baseio.h>

extern "C"
int kputs(const char *str);

extern "C"
size_t brk(size_t newbrk);

extern "C"
void *sbrk(ptrdiff_t increment);

class Test {
public:
    Test() {
        kputs("Test constructor called\n");
    }
};

Test test_goc;

int kmod_main(void) {
    kputs("Hello from kmod_main!\n");
    size_t heap_base = brk(0);

    char buf[256];
    sprintf(buf, "Current brk: %p\n", (void *)heap_base);
    kputs(buf);

    // Will causes an error
    // auto ptr = reinterpret_cast<char *>(heap_base);
    // *ptr = 'c';

    auto ptr = reinterpret_cast<char *>(sbrk(28));
    size_t brk2 = brk(0);
    sprintf(buf, "Current brk: %p\n", (void *)brk2);
    kputs(buf);

    for (int i = 0 ; i < 26 ; i ++)
    {
        *(ptr + i) = 'A' + i;
    }
    *(ptr + 26) = '\n';
    *(ptr + 27) = '\0';
    kputs(ptr);

    // will not cause an error
    auto ptr2 = reinterpret_cast<char *>(brk2 - 1);
    *ptr2 = 'c';

    kputs("This message should be displayed\n");

    // will cause an error
    auto ptr3 = reinterpret_cast<char *>(heap_base + 4096);
    *ptr3 = 'c';

    kputs("This message shouldn't be displayed\n");

    while (true);
    return 0;
}
