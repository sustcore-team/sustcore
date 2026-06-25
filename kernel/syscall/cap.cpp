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
#include <env.h>
#include <logger.h>
#include <object/memory.h>
#include <sustcore/addr.h>
#include <sustcore/capability.h>
#include <syscall/cap.h>
#include <task/scheduler.h>
namespace syscall {
    namespace {
        [[nodiscard]]
        Result<task::TCB *> running_tcb() noexcept {
            auto *current = schd::Scheduler::inst().current_tcb();
            if (current == nullptr || current->task == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return current;
        }
    }  // namespace

    /**
     * @brief 获取当前线程的 capability holder.
     */
    [[nodiscard]]
    static Result<cap::CHolder *> current_holder() noexcept {
        auto tcb_res = running_tcb();
        propagate(tcb_res);
        if (tcb_res.value()->task->cholder == nullptr)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return tcb_res.value()->task->cholder;
    }

    Result<CapIdx> cap_clone(CapIdx src) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto clone_res = holder_res.value()->clone(src);
        propagate(clone_res);
        return clone_res.value();
    }

    Result<bool> cap_downgrade(CapIdx idx, b64 new_perm) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto downgrade_res = holder_res.value()->downgrade(idx, new_perm);
        propagate(downgrade_res);
        return true;
    }

    Result<CapIdx> cap_derive(CapIdx src, b64 new_perm) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto derive_res = holder_res.value()->derive(src, new_perm);
        propagate(derive_res);
        return derive_res.value();
    }

    Result<bool> sys_cap_lookup(CapIdx idx, UBuffer &&info_buf) {
        auto current_tcb_res = running_tcb();
        propagate(current_tcb_res);
        auto *current = current_tcb_res.value();
        auto *holder = current->task->cholder;
        auto cap_res = holder->lookup(idx);
        if (!cap_res.has_value()) {
            if (cap_res.error() == ErrCode::OUT_OF_BOUNDARY) {
                return false;
            }
            propagate_return(cap_res);
        }

        // 将能力类型与权限回填到用户态缓冲区
        CapInfo info{
            .type        = cap_res.value()->payload()->type_id(),
            .permissions = cap_res.value()->perm(),
        };
        memcpy(info_buf.kbuf(), &info, sizeof(info));
        auto commit_res = info_buf.commit_to_user();
        propagate(commit_res);
        return true;
    }

    Result<bool> cap_remove(CapIdx idx) {
        auto holder_res = current_holder();
        propagate(holder_res);

        auto cap_res = holder_res.value()->lookup(idx);
        propagate(cap_res);

        loggers::SYSCALL::DEBUG("移除能力 capability idx=%lu, type=%s", idx,
                                 to_string(cap_res.value()->payload()->type_id()));

        auto remove_res = holder_res.value()->remove(idx);
        propagate(remove_res);
        return true;
    }
}  // namespace syscall
