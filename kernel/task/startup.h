/**
 * @file startup.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 进程启动参数
 * @version alpha-1.0.0
 * @date 2026-05-14
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sustcore/addr.h>

namespace task
{
    // 进程启动时的相关函数
    // 在进入进程前写到寄存器中
    // 一般在 create_process 的最后写入
    struct StartupInfo
    {
        VirAddr heap_vaddr;  // 堆的起始地址
        VirAddr stack_vaddr; // 栈的起始地址
        VirAddr entrypoint;  // 入口点地址
    };
}