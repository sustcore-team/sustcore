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
#include <sustcore/capability.h>
#include <syscall/uaccess.h>

#include <cstddef>

namespace syscall {
    /**
     * @brief 创建进程, 用户路径与 capability 列表已由 dispatcher 预处理.
     */
    [[nodiscard]]
    Result<CapIdx> pcb_create_process(CapIdx pcb_cap, CapIdx image_cap,
                                      UBuffer &&caps_buf, size_t caps_sz,
                                      size_t sched_class,
                                      UBuffer *startup_buf = nullptr,
                                      size_t startup_buf_sz = 0);

    /**
     * @brief 创建线程.
     */
    [[nodiscard]]
    Result<CapIdx> pcb_create_thread(CapIdx pcb_cap, VirAddr entry,
                                     VirAddr stack_addr, size_t stack_size);

    /**
     * @brief fork 当前进程, 子 PCB capability 输出缓冲区已由 dispatcher 预处理.
     */
    [[nodiscard]]
    Result<size_t> pcb_fork(CapIdx pcb_cap, UBuffer &&child_pcb_cap_buf);
    /**
     * @brief 通过 PCB Capability 杀死进程. 
     *
     * syscall 层只 lookup PCB capability; KILL 权限检查由 PCBObject 完成. 
     *
     * @param pcb_cap PCB capability. 
     * @param exit_code 退出码. 
     * @return true 成功; false 失败. 
     */
    [[nodiscard]]
    Result<bool> pcb_kill(CapIdx pcb_cap, int exit_code);
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
    [[nodiscard]]
    Result<bool> pcb_map(CapIdx pcb_cap, CapIdx mem_cap, VirAddr vaddr,
                         PageMan::RWX rwx, cap::MemoryGrowth growth);

    /**
     * @brief execve, 预留 capability 列表已由 dispatcher 预处理.
     */
    [[nodiscard]]
    Result<bool> pcb_execve(CapIdx pcb_cap, CapIdx image_cap,
                            UBuffer &&reserved_buf, size_t reserved_sz,
                            UBuffer *startup_buf = nullptr,
                            size_t startup_buf_sz = 0);

    /**
     * @brief 判断目标 PCB 是否为当前进程.
     */
    [[nodiscard]]
    bool pcb_is_current(CapIdx pcb_cap);

    /**
     * @brief 获取 PCB 对应的 pid.
     */
    [[nodiscard]]
    Result<size_t> get_pid(CapIdx pcb_cap);
}  // namespace syscall
