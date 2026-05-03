/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 主文件
 * @version alpha-1.0.0
 * @date 2026-04-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

extern "C" {
int kputs(const char *str);
void cpu_idle();
}

class Test {
public:
    Test() {
        kputs("Test constructor called");
    }
};

Test test_goc;

int kmod_main() {
    kputs("Here is init module!");
    cpu_idle();
    return 0;
}