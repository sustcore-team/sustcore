/**
 * @file task.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 任务相关系统调用
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <sustcore/addr.h>
#include <sustcore/capability.h>

namespace syscall {
    class UString;

    struct ForkRet {
        CapIdx child_pcb_cap;
        size_t child_pid;
    };

    CapIdx create_process(const UString &path, VirAddr caps_uaddr,
                          size_t caps_sz, size_t sched_class);
    CapIdx create_thread(VirAddr entry, VirAddr stack_addr,
                         size_t stack_size);
    ForkRet fork();
    bool execve(const UString &path, VirAddr reserved_uaddr,
                size_t reserved_sz);
    void exit();
    size_t get_pid(CapIdx pcb_cap);
}  // namespace syscall
