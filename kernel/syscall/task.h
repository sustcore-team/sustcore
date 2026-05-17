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

#include <arch/description.h>
#include <object/memory.h>
#include <sustcore/addr.h>
#include <sustcore/capability.h>

#include <cstddef>

namespace syscall {
    class UString;

    struct ForkRet {
        CapIdx child_pcb_cap;
        size_t child_pid;
    };

    CapIdx pcb_create_process(CapIdx pcb_cap, const UString &path,
                              VirAddr caps_uaddr, size_t caps_sz,
                              size_t sched_class);
    CapIdx pcb_create_thread(CapIdx pcb_cap, VirAddr entry, VirAddr stack_addr,
                             size_t stack_size);
    ForkRet pcb_fork(CapIdx pcb_cap);
    /**
     * @brief 通过 PCB Capability 杀死进程. 
     *
     * syscall 层只 lookup PCB capability; KILL 权限检查由 PCBObject 完成. 
     *
     * @param pcb_cap PCB capability. 
     * @param exit_code 退出码. 
     * @return true 成功; false 失败. 
     */
    bool pcb_kill(CapIdx pcb_cap, int exit_code);
    /**
     * @brief 通过 PCB Capability 将 Memory 映射到目标进程地址空间. 
     *
     * VMCONTEXT 权限由 PCBObject 校验, Memory 映射权限由 MemoryObject 校验. 
     *
     * @param pcb_cap 目标 PCB capability. 
     * @param mem_cap Memory capability. 
     * @param vaddr 目标虚拟地址. 
     * @param rwx 页权限. 
     * @param growth VMA 增长方式. 
     * @return true 成功; false 失败. 
     */
    bool pcb_map(CapIdx pcb_cap, CapIdx mem_cap, VirAddr vaddr,
                 PageMan::RWX rwx, cap::MemoryGrowth growth);
    bool pcb_execve(CapIdx pcb_cap, const UString &path,
                    VirAddr reserved_uaddr, size_t reserved_sz);
    bool pcb_is_current(CapIdx pcb_cap);
    size_t get_pid(CapIdx pcb_cap);
}  // namespace syscall
