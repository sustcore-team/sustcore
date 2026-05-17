/**
 * @file task.cpp
 * @brief PCB/TCB capability objects
 */

#include <logger.h>
#include <object/perm.h>
#include <object/task.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <task/task_struct.h>

namespace cap {
    // 无人引用的 PCB 对象会被放入 TaskManager 的回收队列中等待销毁
    void PCBPayload::destruct() {
        loggers::TASK::INFO("PCBPayload destruct: pid=%d", pcb->pid);
        task::TaskManager::inst().enqueue_recycle(pcb);
        delete this;
    }

    Result<size_t> PCBObject::get_pid() const {
        if (!imply(perm::pcb::GETPID)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _obj->pcb->pid;
    }

    Result<void> PCBObject::kill(int exit_code) const {
        if (!imply(perm::pcb::KILL)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        task::PCB *pcb    = _obj->pcb;
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        bool killing_current =
            current_tcb != nullptr && current_tcb->task == pcb;

        pcb->exit_code = exit_code;
        if (pcb->exiting) {
            void_return();
        }
        pcb->exiting = true;
        if (pcb->cholder != nullptr) {
            pcb->cholder->internal_clear();
        }
        for (auto &tcb : pcb->threads) {
            if (&tcb != current_tcb &&
                tcb.basic_entity.state == ThreadState::READY)
            {
                auto dequeue_res =
                    schd::Scheduler::inst().dequeue(util::nnullforce(&tcb));
                if (!dequeue_res.has_value()) {
                    loggers::TASK::ERROR("pcb kill移除线程失败: tid=%d err=%d",
                                         tcb.tid, dequeue_res.error());
                }
            }
            tcb.basic_entity.state = ThreadState::WAITING;
        }
        if (killing_current) {
            current_tcb->basic_entity.state = ThreadState::WAITING;
            current_tcb->basic_entity
                .template flags_set<schd::SchedMeta::FLAGS_NEED_RESCHED>();
            schd::Scheduler::inst().schedule();
        }
        void_return();
    }

    Result<void> PCBObject::map(MemoryObject &memory, VirAddr vaddr,
                                PageMan::RWX rwx, MemoryGrowth growth) const {
        if (!imply(perm::pcb::VMCONTEXT)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr || _obj->pcb->tmm == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return memory.map_into(*_obj->pcb->tmm, vaddr, rwx, growth);
    }

    Result<task::PCB *> PCBObject::require_new_thread() const {
        if (!imply(perm::pcb::NEW_THREAD)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _obj->pcb;
    }

    Result<task::PCB *> PCBObject::require_new_process() const {
        if (!imply(perm::pcb::NEW_PROCESS)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _obj->pcb;
    }

    Result<task::PCB *> PCBObject::require_execute() const {
        if (!imply(perm::pcb::EXECUTE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _obj->pcb;
    }

    Result<task::PCB *> PCBObject::require_new_process_execute() const {
        if (!imply(perm::pcb::NEW_PROCESS | perm::pcb::EXECUTE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _obj->pcb;
    }
}  // namespace cap
