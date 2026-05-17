/**
 * @file notif.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief notification相关系统调用
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <sustcore/capability.h>

namespace syscall {
    bool wait_notification(CapIdx capidx, size_t idx);
    bool notification_signal(CapIdx capidx, size_t idx, bool state);
    bool check_notification(CapIdx capidx, size_t idx);
    bool notification_create(CapIdx capidx);
}  // namespace syscall
