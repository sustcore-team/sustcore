/**
 * @file pid.h
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief pid 头文件
 * @version alpha-1.0.0
 * @date 2025-11-24
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once
#include <sus/bits.h>

typedef struct {
    umb_t pid;
} PidHandle;

struct PidNode;

typedef struct {
    umb_t current;
    struct PidNode *recycled;
} RecycleAllocator;


/**
 * @brief 初始化PID分配器
 * 
 */
void pid_init(void);

/**
 * @brief 分配一个PID
 * 
 * @return umb_t PID
 */
umb_t pid_alloc(void);

/**
 * @brief 回收一个PID
 * 
 * @param pid PID
 */
void pid_dealloc(umb_t pid);
