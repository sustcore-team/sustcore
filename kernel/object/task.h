/**
 * @file task.h
 * @brief PCB/TCB capability objects
 */

#pragma once

#include <cap/capability.h>
#include <cap/cholder.h>
#include <sustcore/capability.h>

namespace task {
    struct PCB;
    struct TCB;
}  // namespace task

namespace cap {
    struct PCBPayload : public _PayloadHelper<PayloadType::PCB> {
        task::PCB *pcb;

        explicit PCBPayload(task::PCB *pcb) : pcb(pcb) {}
        void destruct() override;
    };

    struct TCBPayload : public _PayloadHelper<PayloadType::TCB> {
        task::TCB *tcb;

        explicit TCBPayload(task::TCB *tcb) : tcb(tcb) {}
    };

    class PCBObject : public CapObj<PCBPayload> {
    public:
        explicit PCBObject(util::nonnull<Capability *> cap)
            : CapObj<PCBPayload>(cap) {}

        Result<size_t> get_pid() const;
        Result<CapIdx> cap_insert(CapIdx src, CHolder &from) const;
    };

    class TCBObject : public CapObj<TCBPayload> {
    public:
        explicit TCBObject(util::nonnull<Capability *> cap)
            : CapObj<TCBPayload>(cap) {}
    };
}  // namespace cap
