/**
 * @file memory.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Memory capability syscalls
 * @version alpha-1.0.0
 * @date 2026-05-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <cap/permission.h>
#include <env.h>
#include <logger.h>
#include <mem/gfp.h>
#include <mem/vma.h>
#include <object/memory.h>
#include <object/perm.h>
#include <object/vfile.h>
#include <syscall/memory.h>
#include <cstring>

namespace {
    [[nodiscard]]
    Result<task::TCB *> running_tcb() noexcept {
        auto *current = schd::Scheduler::inst().current_tcb();
        if (current == nullptr || current->task == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return current;
    }

    [[nodiscard]]
    Result<task::PCB *> running_pcb() noexcept {
        auto tcb_res = running_tcb();
        propagate(tcb_res);
        return tcb_res.value()->task;
    }

    /**
     * @brief 获取当前线程的 capability holder.
     */
    [[nodiscard]]
    Result<cap::CHolder *> current_holder() noexcept {
        auto tcb_res = running_tcb();
        propagate(tcb_res);
        if (tcb_res.value()->task->cholder == nullptr)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return tcb_res.value()->task->cholder;
    }

    /**
     * @brief 查找当前进程 capability 空间中的 Memory payload. 
     */
    [[nodiscard]]
    Result<cap::MemoryPayload *> lookup_memory(
        CapIdx idx, cap::Capability **out_cap = nullptr) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto cap_res = holder_res.value()->lookup(idx);
        propagate(cap_res);
        if (out_cap != nullptr) {
            *out_cap = cap_res.value();
        }
        auto *memory = cap_res.value()->payload_as<cap::MemoryPayload>();
        if (memory == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        return memory;
    }

}  // namespace

namespace syscall {
    Result<CapIdx> mem_create(CapIdx file_cap, size_t memsz, bool shared,
                              bool continuity, cap::MemoryGrowth growth,
                              size_t file_offset) {
        if (shared && growth != cap::MemoryGrowth::FIXED) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        util::owner<cap::Capability *> backing_file(nullptr);
        if (cap::valid(file_cap)) {
            if (shared) {
                unexpect_return(ErrCode::NOT_SUPPORTED);
            }
            auto holder_res = current_holder();
            propagate(holder_res);
            auto file_cap_res = holder_res.value()->lookup(file_cap);
            propagate(file_cap_res);
            if (file_cap_res.value()->payload_as<VFile>() == nullptr) {
                unexpect_return(ErrCode::TYPE_NOT_MATCHED);
            }
            if (!file_cap_res.value()->imply(perm::vfile::READ)) {
                unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
            }
            backing_file =
                util::owner<cap::Capability *>(file_cap_res.value()->clone());
            auto downgrade_res = backing_file->downgrade(perm::vfile::READ);
            if (!downgrade_res.has_value()) {
                delete backing_file.get();
                propagate_return(downgrade_res);
            }
        } else if (file_offset != 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto payload = new cap::MemoryPayload(memsz, shared, continuity, growth,
                                              backing_file, file_offset);
        auto holder_res = current_holder();
        propagate(holder_res);
        auto insert_res = holder_res.value()->insert_to_free(payload);
        if (!insert_res.has_value()) {
            delete payload;
            propagate_return(insert_res);
        }
        return insert_res.value();
    }

    Result<bool> mem_unmap(CapIdx idx, VirAddr vaddr) {
        cap::Capability *cap = nullptr;
        auto memory_res      = lookup_memory(idx, &cap);
        propagate(memory_res);
        auto pcb_res = running_pcb();
        propagate(pcb_res);
        auto *tmm = pcb_res.value()->tmm.get();
        if (tmm == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        cap::MemoryObject obj(util::nnullforce(cap));
        auto remove_res = obj.unmap_from(*tmm, vaddr);
        propagate(remove_res);
        return true;
    }

    Result<bool> mem_resize(CapIdx idx, size_t newsz) {
        cap::Capability *cap = nullptr;
        auto memory_res      = lookup_memory(idx, &cap);
        propagate(memory_res);
        cap::MemoryObject obj(util::nnullforce(cap));
        auto pcb_res    = running_pcb();
        propagate(pcb_res);
        auto *tmm       = pcb_res.value()->tmm.get();
        auto resize_res = obj.resize_in(tmm, newsz);
        propagate(resize_res);
        return true;
    }

    Result<void> mem_query(CapIdx idx, UBuffer &&out_buf) {
        cap::Capability *cap = nullptr;
        auto memory_res      = lookup_memory(idx, &cap);
        propagate(memory_res);
        cap::MemoryObject obj(util::nnullforce(cap));
        auto query_res = obj.query();
        propagate(query_res);

        MemQueryRet ret{query_res.value().memsz, query_res.value().allocated};
        memcpy(out_buf.kbuf(), &ret, sizeof(ret));
        auto commit_res = out_buf.commit_to_user();
        propagate(commit_res);
        void_return();
    }
}  // namespace syscall
