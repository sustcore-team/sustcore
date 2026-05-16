/**
 * @file cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力相关系统调用
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addr.h>
#include <sustcore/capability.h>

namespace syscall {
    bool cap_clone(CapIdx src, CapIdx target);
    bool cap_downgrade(CapIdx idx, b64 new_perm);
    bool cap_derive(CapIdx src, CapIdx target, b64 new_perm);
    bool lookup_cap(CapIdx idx, VirAddr info_uaddr);
    bool cap_remove(CapIdx idx);
}  // namespace syscall
