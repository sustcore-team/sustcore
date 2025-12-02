/**
 * @file proc.h
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-11-24
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

// #include <include/arch/riscv64/int/trap.h>
#include <sus/bits.h>
#include <sus/ctx.h>
#include <task/pid.h>

#define POOL_SIZE 2
#define NPROG     2

typedef enum {
    READY   = 0,
    RUNNING = 1,
    BLOCKED = 2,
    ZOMBIE  = 3,
    UNUSED  = 4,
} ProcState;

typedef struct PCB {
    pid_t pid;
    ProcState state;
    RegCtx *ctx;
    umb_t *kstack;  // 内核栈
    // TODO
} PCB;

extern PCB *proc_pool[NPROG];
extern PCB *cur_proc;
extern PCB *init_proc;

/**
 * @brief 实现 context switch
 *
 * @param old  旧进程 context 结构体指针
 * @param new  新进程 context 结构体指针
 */
// void __switch(RegCtx *old, RegCtx *new);

/**
 * @brief 初始化进程池
 */
void proc_init(void);

/**
 * @brief 调度器 - 从当前进程切换到下一个就绪进程
 */
RegCtx *schedule(RegCtx *old);

PCB *alloc_proc(void);

RegCtx *exit_current_task();

__attribute__((section(".ptest1"))) void worker(void);
