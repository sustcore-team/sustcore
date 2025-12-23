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

void thread_test_1(void) {
    printf("THREAD 1\n");
    while (true) {
        yield(true);
    }
}

void thread_test_2(void) {
    printf("THREAD 2\n");
    while (true) {
        yield(true);
    }
}

void test_2(int a, const char *str) {
    printf("测试函数test_2被调用! %d是%s\n", a, str);

    CapPtr thread_cap_1 = create_thread((void *)thread_test_1, 3);
    if (thread_cap_1.val == 0) {
        printf("创建线程失败!\n");
        return;
    }

    CapPtr thread_cap_2 = create_thread((void *)thread_test_2, 5);
    if (thread_cap_2.val == 0) {
        printf("创建线程失败!\n");
        return;
    }

    while (true);
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