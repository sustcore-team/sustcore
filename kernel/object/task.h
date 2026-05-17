/**
 * @file task.h
 * @brief PCB/TCB capability objects
 */

#pragma once

#include <arch/description.h>
#include <cap/capability.h>
#include <cap/cholder.h>
#include <object/memory.h>
#include <sustcore/capability.h>

namespace task {
    struct PCB;
    struct TCB;
}  // namespace task

namespace cap {
    /**
     * @brief PCB Capability 的 payload. 
     *
     * 持有内核 PCB 指针. 最后一个 PCB capability 被释放时, 
     * payload 会把 PCB 放入 TaskManager 的延迟回收队列. 
     */
    struct PCBPayload : public _PayloadHelper<PayloadType::PCB> {
        /// 关联的进程控制块. 
        task::PCB *pcb;

        /**
         * @brief 构造 PCB payload. 
         *
         * @param pcb 关联 PCB. 
         */
        explicit PCBPayload(task::PCB *pcb) : pcb(pcb) {}
        /**
         * @brief 销毁 payload 并触发 PCB 延迟回收. 
         */
        void destruct() override;
    };

    /**
     * @brief TCB Capability 的 payload. 
     */
    struct TCBPayload : public _PayloadHelper<PayloadType::TCB> {
        /// 关联的线程控制块. 
        task::TCB *tcb;

        /**
         * @brief 构造 TCB payload. 
         *
         * @param tcb 关联 TCB. 
         */
        explicit TCBPayload(task::TCB *tcb) : tcb(tcb) {}
    };

    /**
     * @brief PCB Capability 的操作封装. 
     *
     * 进程相关权限检查集中在该对象中完成, syscall 层只负责 lookup
     * capability 并调用对应方法. 
     */
    class PCBObject : public CapObj<PCBPayload> {
    public:
        /**
         * @brief 从 capability 构造 PCBObject. 
         *
         * @param cap PCB 类型 capability. 
         */
        explicit PCBObject(util::nonnull<Capability *> cap)
            : CapObj<PCBPayload>(cap) {}

        /**
         * @brief 查询进程 PID. 
         *
         * 要求 GETPID 权限. 
         *
         * @return 进程 PID. 
         */
        Result<size_t> get_pid() const;
        /**
         * @brief 杀死关联进程并设置退出码. 
         *
         * 要求 KILL 权限. 若目标是当前进程, 会标记当前线程需要重调度并
         * 立即调用调度器; 若目标是其他进程, 会撤下其 READY 线程并标记为
         * WAITING. 
         *
         * @param exit_code 退出码. 
         */
        Result<void> kill(int exit_code) const;
        /**
         * @brief 将 Memory 映射到该 PCB 的虚拟内存空间. 
         *
         * 要求 VMCONTEXT 权限; Memory 权限由 MemoryObject::map_into 继续校验. 
         *
         * @param memory 要映射的 Memory capability 对象. 
         * @param vaddr 目标虚拟地址. 
         * @param rwx 页权限. 
         * @param growth VMA 请求的增长方式. 
         */
        Result<void> map(MemoryObject &memory, VirAddr vaddr, PageMan::RWX rwx,
                         MemoryGrowth growth) const;
        Result<task::PCB *> require_new_thread() const;
        Result<task::PCB *> require_new_process() const;
        Result<task::PCB *> require_execute() const;
        Result<task::PCB *> require_new_process_execute() const;
    };

    /**
     * @brief TCB Capability 的操作封装. 
     */
    class TCBObject : public CapObj<TCBPayload> {
    public:
        /**
         * @brief 从 capability 构造 TCBObject. 
         *
         * @param cap TCB 类型 capability. 
         */
        explicit TCBObject(util::nonnull<Capability *> cap)
            : CapObj<TCBPayload>(cap) {}
    };
}  // namespace cap
