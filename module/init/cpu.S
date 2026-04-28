/**
 * @file cpu.S
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CPU相关函数
 * @version alpha-1.0.0
 * @date 2026-04-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

.global cpu_idle
cpu_idle:
    # wfi 指令是一个特权指令
    # 在U-Mode下执行wfi会导致非法指令异常
    # 需要在对应的异常处理程序中进行特殊处理, 代为执行wfi指令
    # wfi
    j cpu_idle
