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

void kputs(const char *str);

class Test {
public:
    Test() {
        kputs("Test constructor called");
    }
};

Test test_goc;

int kmod_main(void) {
    kputs("Hello from kmod_main!");
    while (true);
    return 0;
}