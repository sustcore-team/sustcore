/**
 * @file notif.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief notification相关系统调用
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <logger.h>
#include <object/notif.h>
#include <sus/nonnull.h>
#include <sustcore/capability.h>
#include <sustcore/errcode.h>
#include <syscall/notif.h>

namespace syscall {
    static Result<cap::NotificationObject> notif_object(CapIdx capidx) {
        // 统一能力查找与类型校验, 减少各 handler 的重复逻辑
        auto cap_res = cap::CHolder::lookup(capidx);
        propagate(cap_res);
        auto *cap = cap_res.value();
        if (cap->payload()->type_id() != PayloadType::NOTIF) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        return cap::NotificationObject(util::nnullforce(cap));
    }

    // TODO: allow syscall to directly return an result,
    // and when returning an error, let the return value to be -errcode,
    // and automatically log the error in the syscall_entrance function,
    // to avoid repetitive error handling code in each syscall handler.
    bool wait_notification(CapIdx capidx, size_t idx) {
        auto notif_res = notif_object(capidx).and_then(
            [idx](cap::NotificationObject obj) { return obj.wait(idx); });
        if (!notif_res.has_value()) {
            loggers::SYSCALL::ERROR("等待notification失败: err=%s",
                                    to_cstring(notif_res.error()));
            return false;
        }
        return notif_res.value();
    }

    bool notification_signal(CapIdx capidx, size_t idx, bool state) {
        auto set_res = notif_object(capidx).and_then(
            [idx, state](cap::NotificationObject obj) {
                return obj.set(idx, state);
            });
        if (!set_res.has_value()) {
            loggers::SYSCALL::ERROR("设置notification失败: err=%s",
                                    to_cstring(set_res.error()));
            return false;
        }
        return set_res.value();
    }

    bool check_notification(CapIdx capidx, size_t idx) {
        auto query_res = notif_object(capidx).and_then(
            [idx](cap::NotificationObject obj) { return obj.query(idx); });
        if (!query_res.has_value()) {
            loggers::SYSCALL::ERROR("查询notification失败: err=%d",
                                    query_res.error());
            return false;
        }
        return query_res.value();
    }

    bool notification_create(CapIdx capidx) {
        auto create_res =
            cap::CHolder::create<cap::NotificationPayload>(capidx);
        if (!create_res.has_value()) {
            loggers::SYSCALL::ERROR("创建notification失败: err=%s",
                                    to_cstring(create_res.error()));
            return false;
        }
        return true;
    }
}  // namespace syscall
