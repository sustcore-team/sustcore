/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用
 * @version alpha-1.0.0
 * @date 2026-05-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/nonnull.h>
#include <sus/types.h>
#include <syscall/packs.h>
#include <sustcore/errcode.h>
#include <arch/description.h>
#include <task/task_struct.h>

namespace syscall {
    const char *name_of(b64 sysno);

    /**
     * @brief 同步 syscall 分发入口.
     *
     * @param tcb 当前系统调用所属线程.
     * @param trap_context 当前系统调用对应的 trap 上下文.
     * @param args 已由架构层解析完成的参数包.
     * @return RetPack 同步执行结果.
     */
    [[nodiscard]]
    RetPack dispatch_sync(util::nonnull<task::TCB *> tcb,
                          util::nonnull<Context *> trap_context,
                          const ArgPack &args);

    /**
     * @brief 处理一次来自用户态 ECALL 的通用 syscall 生命周期.
     *
     * @param tcb 当前系统调用所属线程.
     * @param args 已由架构层解析完成的参数包.
     */
    void handle_user_ecall(util::nonnull<task::TCB *> tcb,
                           util::nonnull<Context *> trap_context,
                           const ArgPack &args) noexcept;
}  // namespace syscall
