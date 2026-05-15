/**
 * @file task.cpp
 * @brief PCB/TCB capability objects
 */

#include <object/task.h>
#include <perm/perm.h>
#include <task/task_struct.h>

namespace cap {
    Result<size_t> PCBObject::get_pid() const {
        if (!imply(perm::pcb::GETPID)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        return _obj->pcb->pid;
    }

    Result<CapIdx> PCBObject::cap_insert(CapIdx src, CHolder &from) const {
        if (!imply(perm::pcb::CAP_INSERT)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (_obj->pcb == nullptr || _obj->pcb->cholder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto src_res = from.internal_lookup(src);
        propagate(src_res);
        Capability *src_cap = src_res.value();
        if (!src_cap->imply(perm::basic::CLONE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        CHolder *target = _obj->pcb->cholder;
        auto slot_res   = target->internal_lookup_freeslot();
        propagate(slot_res);
        CapIdx target_idx = slot_res.value();

        auto insert_res =
            target->internal_insert(target_idx, src_cap->payload(), src_cap->perm());
        propagate(insert_res);
        return target_idx;
    }
}  // namespace cap
