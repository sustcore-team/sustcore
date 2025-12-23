/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用相关(libkmod特定函数)
 * @version alpha-1.0.0
 * @date 2025-12-07
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <sus/capability.h>

/**
 * @brief 初始化PID-能力哈希表
 * 
 */
void init_proc_cap_table(void);

/**
 * @brief 获得能力对应的进程ID
 * 
 * @param cap 进程能力
 * @return int 进程ID
 */
int get_pid(CapPtr cap);

/**
 * @brief 获得指定pid的进程能力
 * 
 * @param pid 进程ID
 * @return CapPtr 进程能力, 未找到则val为0
 */
CapPtr get_proc_cap(int pid);

/**
 * @brief 插入进程能力到哈希表
 * 
 * @param pid 进程ID
 * @param cap 进程能力
 */
void insert_proc_cap(int pid, CapPtr cap);