/**
 * @file offsets.h
 * @brief 汇编 trap 入口使用的 CpuLocal 固定前缀偏移。
 */

#pragma once

#define CPU_LOCAL_CURRENT_KERNEL_STACK_TOP 16
#define CPU_LOCAL_TRAP_SAVED_SP            40
#define CPU_LOCAL_TRAP_SAVED_T0            48
#define CPU_LOCAL_TRAP_SAVED_T1            56
#define CPU_LOCAL_TRAP_SAVED_TP            64
