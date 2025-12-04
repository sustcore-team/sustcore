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

#include <sus/bits.h>
#include <sus/syscall.h>
#include <basec/baseio.h>
#include <string.h>

bool test_prime(int k) {
    if (k <= 1) return false;
    for (int i = 2 ; i * i <= k ; i ++) {
        if (k % i == 0) return false;
    }
    return true;
}

bool test_square(int k) {
    int root = 0;
    while (root * root < k) {
        root++;
    }
    return (root * root == k);
}

void test1(void) {
    for (int i = 2'0000 ; i < 2'2500 ; i ++) {
        if (test_square(i)) {
            printf("找到一个平方数: %d\n", i);
        }
        // 强行延长运行视角 使效果更明显
        for (volatile int j = 0 ; j < 40000 ; j ++);
    }
}

void test2(void) {
    for (int i = 2'2500 ; i < 2'5000 ; i ++) {
        if (test_square(i)) {
            printf("找到一个平方数: %d\n", i);
        }
        // 强行延长运行视角 使效果更明显
        for (volatile int j = 0 ; j < 40000 ; j ++);
    }
}

void test3(void) {
    for (int i = 2'5000 ; i < 2'7500 ; i ++) {
        if (test_square(i)) {
            printf("找到一个平方数: %d\n", i);
        }
        // 强行延长运行视角 使效果更明显
        for (volatile int j = 0 ; j < 40000 ; j ++);
    }
}

void test4(void) {
    for (int i = 2'7500 ; i < 3'0000 ; i ++) {
        if (test_square(i)) {
            printf("找到一个平方数: %d\n", i);
        }
        // 强行延长运行视角 使效果更明显
        for (volatile int j = 0 ; j < 40000 ; j ++);
    }
}

extern umb_t arg[8];

int kmod_main(void) {
    umb_t pid = arg[1];
    printf("测试模块启动! PID=%d\n", pid);
    switch (pid)
    {
    case 1:
        test1();
        break;
    case 2:
        test2();
        break;
    case 3:
        test3();
        break;
    default:
        test4();
        break;
    }
    return 0;
}