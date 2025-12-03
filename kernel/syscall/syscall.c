/**
 * @file syscall.c
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-12-02
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <basec/logger.h>
#include <syscall/syscall.h>
#include <task/proc.h>

void syscall_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                     InterruptContextRegisterList *ctx) {
    log_debug("系统调用处理程序: scause=0x%lx, sepc=0x%lx, stval=0x%lx, from pid: %d",
              scause.value, sepc, stval, cur_proc == nullptr ? -1 : cur_proc->pid);
    umb_t syscall_num  = ctx->regs[16];  // a7 寄存器保存系统调用号
    umb_t ret          = 0;
    ctx->sepc         += 4;  // 跳过 ecall 指令
    switch (syscall_num) {
        case SYS_EXIT:
            log_info("进程调用 exit 系统调用");
            // 设置为ZOMBIET态, 从而在之后的调度中被清理
            cur_proc->state = PS_ZOMBIE;
            break;
        default: log_info("未知系统调用号: %lu", syscall_num); break;
    }
    // ret       = test_syscall();
    log_info("系统调用返回值: %lu", ret);
    ctx->regs[9] = ret;  // 将返回值放入 a0 寄存器
}

umb_t test_syscall() {
    for (int i = 0; i < 10; i++) {
        log_info("syscall test: %d", i);
    }
    return 1;
}