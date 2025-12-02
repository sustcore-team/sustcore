/**
 * @file syscall.h
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-12-02
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once
#include <sus/bits.h>
#include <sus/ctx.h>

#define SYS_EXIT 93

void syscall_handler(csr_scause_t scause, umb_t sepc, umb_t stval,
                     InterruptContextRegisterList *ctx);

umb_t test_syscall();