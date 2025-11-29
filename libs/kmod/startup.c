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

#include <kmod/syscall.h>
#include <sus/bits.h>

extern int kmod_main(void);

static Capability device_cap;

Capability sa_get_device(void) {
    return device_cap;
}

void init(int heap_ptr) {
    // 初始化堆指针
    device_cap = (Capability){
        .raw = 0x00000000,  // TODO: 从系统参数获取设备能力
    };
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

void _start(void) {
    // 根据约定, r1寄存器保存heap指针
    register umb_t heap_ptr asm("r1");
    init(heap_ptr);
    // 调用主函数
    int ret = kmod_main();
    // 结束
    terminate(ret);
    // unreachable
    while (true);
}