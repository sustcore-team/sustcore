/**
 * @file exception.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 异常处理程序头文件
 * @version alpha-1.0.0
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <sus/types.h>

/**
 * @brief 中断向量表模式(DIRECT)
 *
 */
#define DIRECT   0

/**
 * @brief 中断向量表模式(VECTORED)
 *
 */
#define VECTORED 1

/**
 * @brief 中断向量表模式开关
 *
 */
#define IVT_MODE VECTORED

#if IVT_MODE == VECTORED

/**
 * @brief IVT表项数
 *
 */
#define IVT_ENTRIES (16)
/**
 * @brief IVT表
 *
 */
extern dword IVT[IVT_ENTRIES];

#endif

/**
 * @brief 初始化IVT
 *
 */
void init_ivt(void);

/**
 * @brief 启用中断
 *
 */
void sti(void);