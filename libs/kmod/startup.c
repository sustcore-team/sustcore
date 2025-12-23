/**
 * @file startup.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核模块启动文件
 * @version alpha-1.0.0
 * @date 2025-11-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <alloc.h>
#include <startup.h>
#include <kmod/syscall.h>
#include <sus/bits.h>
#include <syscall.h>

extern int kmod_main(void);

static CapPtr device_cap;
CapPtr main_thread_cap;
CapPtr pcb_cap;
CapPtr default_notif_cap;

CapPtr sa_get_device(void) {
    return device_cap;
}

void init(umb_t heap_ptr) {
    // 初始化堆指针
    init_malloc((void *)heap_ptr);
    device_cap = INVALID_CAP_PTR; // TODO: 从系统参数获取设备能力
}

void terminate(int code) {
    if (code != 0) {
        // 错误退出
        exit(code);
        // unreachable
        while (true);
    }
    // 正常退出
    exit(0);
    // unreachable
    while (true);
}

umb_t arg[8];

void _start(void) {
    register umb_t arg0 asm("a0");
    register umb_t arg1 asm("a1");
    register umb_t arg2 asm("a2");
    register umb_t arg3 asm("a3");
    register umb_t arg4 asm("a4");
    register umb_t arg5 asm("a5");
    register umb_t arg6 asm("a6");
    register umb_t arg7 asm("a7");

    arg[0] = arg0;
    arg[1] = arg1;
    arg[2] = arg2;
    arg[3] = arg3;
    arg[4] = arg4;
    arg[5] = arg5;
    arg[6] = arg6;
    arg[7] = arg7;

    // 根据约定
    // a0寄存器保存PCB能力
    // a1寄存器保存heap指针
    pcb_cap.val     = arg[0];
    umb_t heap_ptr  = arg[1];
    main_thread_cap.val = arg[2];
    default_notif_cap.val   = arg[3];

    init(heap_ptr);

    init_proc_cap_table();
    insert_proc_cap(get_current_pid(), pcb_cap);

    // 调用主函数
    int ret = kmod_main();
    // 结束
    terminate(ret);
    // unreachable
    while (true);
}