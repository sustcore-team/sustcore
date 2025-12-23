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
#include <kmod/system_args.h>
#include <string.h>
#include <sus/bits.h>

void thread_test_1(void) {
    register umb_t arg0 asm("a0");
    CapPtr thread_cap;
    thread_cap.val = arg0;
    printf("THREAD 1\n");

    // 测试通知
    CapPtr notif = get_notification_cap();

    // 监听32号通知
    printf("线程1等待通知32号\n");
    wait_notification(thread_cap, notif, 32);
    printf("线程1收到通知32号\n");
    // 发送64号通知
    printf("线程1发送通知64号\n");
    notification_set(notif, 64);
    printf("线程1等待通知32号\n");
    wait_notification(thread_cap, notif, 32);
    printf("线程1收到通知32号\n");

    // 等待128号通知(不会被发送)
    wait_notification(thread_cap, notif, 128);
}

void thread_test_2(void) {
    register umb_t arg0 asm("a0");
    CapPtr thread_cap;
    thread_cap.val = arg0;
    printf("THREAD 2\n");

    // 测试通知
    CapPtr notif = get_notification_cap();

    printf("线程2发送通知32号\n");
    notification_set(notif, 32);

    printf("线程2等待通知64号\n");
    wait_notification(thread_cap, notif, 64);
    printf("线程2收到通知64号\n");
    printf("线程2发送通知96号\n");
    notification_set(notif, 96);

    // 等待128号通知(不会被发送)
    wait_notification(thread_cap, notif, 128);
}

void test_2(int a, const char *str) {
    printf("测试函数test_2被调用! %d是%s\n", a, str);

    CapPtr main_thread_cap = get_main_thread_cap();

    CapPtr thread_cap_1 = create_thread((void *)thread_test_1, 129);
    if (thread_cap_1.val == 0) {
        printf("创建线程失败!\n");
        return;
    }

    CapPtr thread_cap_2 = create_thread((void *)thread_test_2, 129);
    if (thread_cap_2.val == 0) {
        printf("创建线程失败!\n");
        return;
    }

    CapPtr notif = get_notification_cap();
    wait_notification(main_thread_cap, notif, 96);
    printf("主线程收到通知96号\n");
    printf("主线程发送通知32号\n");
    notification_set(notif, 32);

    // 等待128号通知(不会被发送)
    wait_notification(main_thread_cap, notif, 128);
}

int kmod_main(void) {
    umb_t pid = get_current_pid();
    printf("测试模块启动! PID=%d\n", pid);

    test_2(2, "A");


    return 0;
}