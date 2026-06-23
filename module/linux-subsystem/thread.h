/**
 * @file thread.h
 * @author theflysong
 * @brief linux subsystem 线程辅助接口
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

#include <sustcore/capability.h>

CapIdx linuxss_create_thread(addr_t entrypoint);
