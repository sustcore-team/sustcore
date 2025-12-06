/**
 * @file main.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试模块主文件
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <basec/baseio.h>
#include <kmod/syscall.h>
#include <string.h>
#include <sus/bits.h>

void test_1() {
    printf("测试函数test_1被调用!\n");
}

void test_2(int a, const char *str) {
    printf("测试函数test_2被调用! %d是%s\n", a, str);
}

int kmod_main(void) {
    umb_t pid = get_current_pid();
    printf("测试模块启动! PID=%d\n", pid);

    // 执行fork测试
    int ret = fork();
    printf("fork调用返回值: %d\n", ret);

    if (ret == 0) {
        // 子进程
        printf("子进程: PID=%d\n", get_current_pid());
        test_2(42, "宇宙最终问题的答案");
    } else {
        // 父进程
        test_1();
    }

    return 0;
}