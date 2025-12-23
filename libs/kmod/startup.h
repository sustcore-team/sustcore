/**
 * @file startup.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核模块启动文件接口
 * @version alpha-1.0.0
 * @date 2025-12-06
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <kmod/syscall.h>

extern CapPtr pcb_cap;
extern CapPtr main_thread_cap;
extern CapPtr default_notif_cap;