/**
 * @file cap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力相关系统调用
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <logger.h>
#include <sustcore/addr.h>
#include <sustcore/capability.h>
#include <syscall/cap.h>
#include <syscall/uaccess.h>

namespace syscall {
    // 将 capability 操作的 Result 映射为 bool，避免入口重复处理
    bool cap_clone(CapIdx src, CapIdx target) {
        auto clone_res = cap::CHolder::clone(target, src);
        if (!clone_res.has_value()) {
            loggers::SYSCALL::ERROR("clone capability失败: err=%d",
                                    clone_res.error());
            return false;
        }
        return true;
    }

    bool cap_downgrade(CapIdx idx, b64 new_perm) {
        auto downgrade_res = cap::CHolder::downgrade(idx, new_perm);
        if (!downgrade_res.has_value()) {
            loggers::SYSCALL::ERROR("downgrade capability失败: err=%d",
                                    downgrade_res.error());
            return false;
        }
        return true;
    }

    bool cap_derive(CapIdx src, CapIdx target, b64 new_perm) {
        auto derive_res = cap::CHolder::derive(target, src, new_perm);
        if (!derive_res.has_value()) {
            loggers::SYSCALL::ERROR("derive capability失败: err=%d",
                                    derive_res.error());
            return false;
        }
        return true;
    }

    bool lookup_cap(CapIdx idx, VirAddr info_uaddr) {
        if (!info_uaddr.nonnull()) {
            return false;
        }

        auto cap_res = cap::CHolder::lookup(idx);
        if (!cap_res.has_value()) {
            if (cap_res.error() == ErrCode::OUT_OF_BOUNDARY) {
                return false;
            }
            loggers::SYSCALL::ERROR("lookup_cap失败: err=%d", cap_res.error());
            return false;
        }

        // 将能力类型与权限回填到用户态缓冲区
        CapInfo info{
            .type        = cap_res.value()->payload()->type_id(),
            .permissions = cap_res.value()->perm(),
        };
        UBuffer info_buf(info_uaddr, sizeof(info));
        memcpy(info_buf.kbuf(), &info, sizeof(info));
        info_buf.sync_to_user();
        return true;
    }

    bool cap_remove(CapIdx idx) {
        auto remove_res = cap::CHolder::remove(idx);
        if (!remove_res.has_value()) {
            loggers::SYSCALL::ERROR("cap_remove失败: idx=%p err=%d", idx,
                                    remove_res.error());
            return false;
        }
        return true;
    }
}  // namespace syscall
