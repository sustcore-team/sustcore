/**
 * @file task.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 任务相关系统调用
 * @version alpha-1.0.0
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <env.h>
#include <logger.h>
#include <mem/gfp.h>
#include <mem/vma.h>
#include <object/memory.h>
#include <object/perm.h>
#include <object/task.h>
#include <object/vfile.h>
#include <schd/schdbase.h>
#include <sus/nonnull.h>
#include <sus/raii.h>
#include <sustcore/capability.h>
#include <syscall/task.h>
#include <syscall/uaccess.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <vfs/vfs.h>

#include <cassert>
#include <cstring>
#include <sys/wait.h>

namespace syscall {
    namespace {
        constexpr const char *POSIX_SUBSYSTEM_IMAGE = "/initrd/linux-subsystem.mod";
        constexpr size_t TCB_WAIT_WNOHANG           = 1;

        [[nodiscard]]
        Result<task::TCB *> running_tcb() noexcept {
            auto *current = schd::Scheduler::inst().current_tcb();
            if (current == nullptr || current->task == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return current;
        }

        [[nodiscard]]
        int encode_linux_wait_status(int exit_code) noexcept {
            return __W_EXITCODE(exit_code & 0xff, 0);
        }

        Result<void> write_wait_status(UBuffer *status_buf,
                                       cap::PCBPayload *pcb_payload) {
            if (status_buf == nullptr) {
                void_return();
            }
            if (pcb_payload == nullptr || pcb_payload->pcb == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }

            int status = encode_linux_wait_status(pcb_payload->pcb->exit_code);
            memcpy(status_buf->kbuf(), &status, sizeof(status));
            auto commit_res = status_buf->commit_to_user();
            propagate(commit_res);
            void_return();
        }
    }  // namespace

    /**
     * @brief 获取当前线程.
     */
    /**
     * @brief 获取当前线程所属的 capability holder.
     */
    static Result<cap::CHolder *> current_holder() {
        auto tcb_res = running_tcb();
        propagate(tcb_res);
        if (tcb_res.value()->task->cholder == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return tcb_res.value()->task->cholder;
    }

    /**
     * @brief 查找当前进程 capability 空间中的 PCB payload.
     */
    static Result<cap::PCBPayload *> lookup_pcb(CapIdx idx,
                                                cap::Capability **out_cap) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto cap_res = holder_res.value()->lookup(idx);
        propagate(cap_res);
        if (out_cap != nullptr) {
            *out_cap = cap_res.value();
        }
        auto *pcb = cap_res.value()->payload_as<cap::PCBPayload>();
        if (pcb == nullptr || pcb->pcb == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        return pcb;
    }

    static Result<cap::MemoryPayload *> lookup_memory_in_holder(
        cap::CHolder *holder, CapIdx idx, cap::Capability **out_cap) {
        if (holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto cap_res = holder->lookup(idx);
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

    static Result<cap::TCBPayload *> lookup_tcb(CapIdx idx,
                                                cap::Capability **out_cap) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto cap_res = holder_res.value()->lookup(idx);
        propagate(cap_res);
        if (out_cap != nullptr) {
            *out_cap = cap_res.value();
        }
        auto *tcb = cap_res.value()->payload_as<cap::TCBPayload>();
        if (tcb == nullptr || tcb->tcb == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        return tcb;
    }

    static Result<cap::Capability *> lookup_vfile(CapIdx idx) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto cap_res = holder_res.value()->lookup(idx);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (!cap_res.value()->imply(perm::vfile::EXEC)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        return cap_res.value();
    }

    /**
     * @brief 将父进程指定 capability 按相同 CapIdx 放入子 CHolder.
     *
     * 支持两类基础语义:
     * - `CLONE`: 子进程获得 payload 副本/共享引用, 父进程保留原 capability.
     * - `MIGRATE` / `MIGRATE_ONCE`: 子进程获得同一 payload 的 capability,
     *   若为 `MIGRATE_ONCE` 则目标 capability 自动清除此位, 父进程源 slot 被消费.
     *
     * 对象自身权限检查仍由对应 CapObj 方法完成.
     */
    static Result<void> copy_initial_caps_in_place(cap::CHolder *src_holder,
                                                   TaskMemoryManager *src_tmm,
                                                   cap::CHolder *dst_holder,
                                                   UBuffer *caps_buf,
                                                   size_t caps_sz) {
        if (src_holder == nullptr || dst_holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (caps_sz == 0) {
            void_return();
        }

        if (caps_buf == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *caps = reinterpret_cast<CapIdx *>(caps_buf->kbuf());
        for (size_t i = 0; i < caps_sz; ++i) {
            CapIdx idx   = caps[i];
            auto src_res = src_holder->lookup(idx);
            propagate(src_res);
            cap::Capability *src_cap = src_res.value();

            cap::Payload *payload = nullptr;
            b64 delivered_perm    = src_cap->perm();
            bool moved_cap        = false;

            if (src_cap->imply(perm::basic::CLONE)) {
                auto *src_payload = src_cap->payload();
                payload           = src_payload->clone_payload();
            } else if (src_cap->imply(perm::basic::MIGRATE) ||
                       src_cap->imply(perm::basic::MIGRATE_ONCE))
            {
                payload        = src_cap->payload();
                delivered_perm = src_cap->perm() & ~perm::basic::MIGRATE_ONCE;
                moved_cap      = true;
            } else {
                unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
            }

            auto insert_res = dst_holder->insert(idx, payload, delivered_perm);
            if (!insert_res.has_value()) {
                if (!moved_cap && payload != src_cap->payload()) {
                    payload->destruct();
                }
                propagate_return(insert_res);
            }

            if (moved_cap) {
                auto remove_res = src_holder->remove(idx);
                if (!remove_res.has_value()) {
                    auto rollback_res = dst_holder->remove(idx);
                    assert(rollback_res.has_value());
                    propagate_return(remove_res);
                }
                continue;
            }

            auto *memory = src_cap->payload_as<cap::MemoryPayload>();
            if (memory != nullptr && !memory->shared && src_tmm != nullptr) {
                auto cow_res = src_tmm->protect_memory_cow(memory);
                if (!cow_res.has_value()) {
                    auto remove_res = dst_holder->remove(idx);
                    assert(remove_res.has_value());
                    propagate_return(cow_res);
                }
            }
        }
        void_return();
    }

    static Result<schd::ClassType> parse_user_sched_class(size_t value) {
        switch (static_cast<schd::ClassType>(value)) {
            case schd::ClassType::RT:
            case schd::ClassType::RR:
            case schd::ClassType::FCFS:
                return static_cast<schd::ClassType>(value);
            case schd::ClassType::INIT:
            case schd::ClassType::IDLE:
            case schd::ClassType::BOT:
                unexpect_return(ErrCode::INVALID_PARAM);
            default: unexpect_return(ErrCode::INVALID_PARAM);
        }
    }

    [[nodiscard]]
    static Result<CapIdx> find_exited_pcb_cap(const std::vector<CapIdx> &pcbs) {
        if (pcbs.empty()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        for (auto pcb_cap : pcbs) {
            cap::Capability *pcb_cap_obj = nullptr;
            auto pcb_res                 = lookup_pcb(pcb_cap, &pcb_cap_obj);
            if (!pcb_res.has_value()) {
                continue;
            }
            if (pcb_res.value()->pcb->exiting) {
                return pcb_cap;
            }
        }
        return cap::null;
    }

    Result<CapIdx> pcb_create_process(CapIdx pcb_cap, CapIdx image_cap,
                                      size_t sched_class,
                                      const StartupArguments &startup) {
        loggers::SYSCALL::DEBUG(
            "创建进程: pcb=%p image_cap=%p, caps_sz=%u, "
            "sched_class=%u",
            pcb_cap, image_cap, startup.caps.size(), sched_class);

        cap::Capability *pcb_cap_obj = nullptr;
        auto pcb_res                 = lookup_pcb(pcb_cap, &pcb_cap_obj);
        propagate(pcb_res);
        cap::PCBObject pcb_obj(util::nnullforce(pcb_cap_obj));
        auto parent_res = pcb_obj.require_new_process_execute();
        propagate(parent_res);
        task::PCB *parent_pcb = parent_res.value();
        if (parent_pcb->cholder == nullptr || parent_pcb->tmm == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto sched_res = parse_user_sched_class(sched_class);
        propagate(sched_res);
        auto image_res = lookup_vfile(image_cap);
        propagate(image_res);

        // 1) 获取当前 CSpace 作为能力来源
        auto current_holder_res = current_holder();
        propagate(current_holder_res);
        cap::CHolder *current_holder = current_holder_res.value();

        auto child_holder_res = cap::CHolderManager::inst().create_holder();
        propagate(child_holder_res);
        cap::CHolder *child_holder = child_holder_res.value();
        auto holder_guard          = util::Guard([child_holder]() {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(child_holder->id());
            assert(rm_res.has_value());
        });

        if (!startup.caps.empty()) {
            UBuffer caps_buf(VirAddr::null,
                             startup.caps.size() * sizeof(CapIdx));
            memcpy(caps_buf.kbuf(), startup.caps.data(),
                   startup.caps.size() * sizeof(CapIdx));
            auto copy_res = copy_initial_caps_in_place(
                parent_pcb->cholder, parent_pcb->tmm.get(), child_holder,
                &caps_buf, startup.caps.size());
            propagate(copy_res);
        }

        auto child_image_cap_res = child_holder->insert_to_free(
            image_res.value()->payload(), perm::vfile::EXEC);
        propagate(child_image_cap_res);

        // 2) 使用已预配置的 CHolder 加载子进程 ELF
        auto load_res = task::TaskManager::inst().load_elf_into(
            child_image_cap_res.value(), child_holder, sched_res.value(),
            startup.argv, startup.envp, startup.bsargv);
        propagate(load_res);
        holder_guard.release();
        auto pcb_guard = util::Guard([&]() {
            if (load_res.has_value()) {
                auto pcb = load_res.value();
                loggers::SYSCALL::INFO("清理已加载的PCB对象: pid=%d", pcb->pid);
                // TODO: 调用 task manager 的接口来删除 PCB 对象
                // task::TaskManager::inst().remove_pcb(pcb->pid);
            }
        });

        auto pcb               = load_res.value();
        auto child_pcb_cap_res = pcb->cholder->lookup(pcb->pcb_cap);
        propagate(child_pcb_cap_res);

        // 4) 返回子进程 PCB 能力给调用方
        auto ret_insert_res = current_holder->insert_to_free(
            child_pcb_cap_res.value()->payload(),
            child_pcb_cap_res.value()->perm());
        propagate(ret_insert_res);

        loggers::SYSCALL::DEBUG("创建进程成功: image_cap=%p, pid=%d", image_cap,
                                pcb->pid);
        pcb_guard.release();
        return ret_insert_res.value();
    }

    Result<CapIdx> pcb_create_linux_process(CapIdx pcb_cap, CapIdx image_cap,
                                            size_t sched_class,
                                            const StartupArguments &startup) {
        loggers::SYSCALL::DEBUG(
            "创建POSIX进程: pcb=%p image_cap=%p, caps_sz=%u, "
            "sched_class=%u",
            pcb_cap, image_cap, startup.caps.size(), sched_class);

        cap::Capability *pcb_cap_obj = nullptr;
        auto pcb_res                 = lookup_pcb(pcb_cap, &pcb_cap_obj);
        propagate(pcb_res);
        cap::PCBObject pcb_obj(util::nnullforce(pcb_cap_obj));
        auto parent_res = pcb_obj.require_new_process_execute();
        propagate(parent_res);
        task::PCB *parent_pcb = parent_res.value();
        if (parent_pcb->cholder == nullptr || parent_pcb->tmm == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto sched_res = parse_user_sched_class(sched_class);
        propagate(sched_res);
        auto image_res = lookup_vfile(image_cap);
        propagate(image_res);

        auto current_holder_res = current_holder();
        propagate(current_holder_res);
        cap::CHolder *current_holder = current_holder_res.value();

        auto child_holder_res = cap::CHolderManager::inst().create_holder();
        propagate(child_holder_res);
        cap::CHolder *child_holder = child_holder_res.value();
        auto holder_guard          = util::Guard([child_holder]() {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(child_holder->id());
            assert(rm_res.has_value());
        });

        if (!startup.caps.empty()) {
            UBuffer caps_buf(VirAddr::null,
                             startup.caps.size() * sizeof(CapIdx));
            memcpy(caps_buf.kbuf(), startup.caps.data(),
                   startup.caps.size() * sizeof(CapIdx));
            auto copy_res = copy_initial_caps_in_place(
                parent_pcb->cholder, parent_pcb->tmm.get(), child_holder,
                &caps_buf, startup.caps.size());
            propagate(copy_res);
        }

        auto child_image_cap_res = child_holder->insert_to_free(
            image_res.value()->payload(), perm::vfile::EXEC);
        propagate(child_image_cap_res);
        auto subsystem_cap_res =
            VFS::inst().open(POSIX_SUBSYSTEM_IMAGE, *child_holder);
        propagate(subsystem_cap_res);

        auto create_res = task::TaskManager::inst().load_linux_elf_into(
            child_image_cap_res.value(), child_holder, subsystem_cap_res.value(),
            sched_res.value(), startup.argv, startup.envp, startup.bsargv);
        propagate(create_res);
        holder_guard.release();

        auto *pcb = create_res.value().get();
        auto child_pcb_cap_res = pcb->cholder->lookup(pcb->pcb_cap);
        propagate(child_pcb_cap_res);
        auto ret_insert_res = current_holder->insert_to_free(
            child_pcb_cap_res.value()->payload(),
            child_pcb_cap_res.value()->perm());
        propagate(ret_insert_res);

        loggers::SYSCALL::DEBUG("创建POSIX进程成功: image_cap=%p, pid=%d",
                               image_cap, pcb->pid);
        return ret_insert_res.value();
    }

    Result<CapIdx> pcb_create_thread(CapIdx pcb_cap, VirAddr entry,
                                     VirAddr stack_addr, size_t stack_size) {
        cap::Capability *cap = nullptr;
        auto pcb_res         = lookup_pcb(pcb_cap, &cap);
        propagate(pcb_res);
        cap::PCBObject obj(util::nnullforce(cap));
        auto target_res = obj.require_new_thread();
        propagate(target_res);
        auto current_tcb_res = running_tcb();
        propagate(current_tcb_res);
        auto *current = current_tcb_res.value();
        if (current->task != target_res.value()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto thread_res = task::TaskManager::inst().create_thread_current(
            entry, stack_addr, stack_size);
        propagate(thread_res);
        return thread_res.value();
    }

    Result<CapIdx> tcb_wait(CapIdx tcb_cap, const std::vector<CapIdx> &pcbs,
                            UBuffer *status_buf, size_t options) {
        if (options != 0 && options != TCB_WAIT_WNOHANG) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        cap::Capability *tcb_cap_obj = nullptr;
        auto tcb_res                 = lookup_tcb(tcb_cap, &tcb_cap_obj);
        propagate(tcb_res);
        cap::TCBObject tcb_obj(util::nnullforce(tcb_cap_obj));
        auto current_tcb_res = tcb_obj.require_current();
        propagate(current_tcb_res);

        auto exited_res = find_exited_pcb_cap(pcbs);
        propagate(exited_res);
        if (exited_res.value() != cap::null ||
            options == TCB_WAIT_WNOHANG)
        {
            if (exited_res.value() != cap::null) {
                auto pcb_res = lookup_pcb(exited_res.value(), nullptr);
                propagate(pcb_res);
                auto status_res =
                    write_wait_status(status_buf, pcb_res.value());
                propagate(status_res);
            }
            return exited_res.value();
        }

        auto wait_res = wait_event(task::task_exit_wait_wd(), ({
            auto __exited_res = find_exited_pcb_cap(pcbs);
            __exited_res.has_value() && __exited_res.value() != cap::null;
        }));
        propagate(wait_res);
        auto final_res = find_exited_pcb_cap(pcbs);
        propagate(final_res);
        if (final_res.value() != cap::null) {
            auto pcb_res = lookup_pcb(final_res.value(), nullptr);
            propagate(pcb_res);
            auto status_res = write_wait_status(status_buf, pcb_res.value());
            propagate(status_res);
        }
        return final_res.value();
    }

    Result<size_t> pcb_fork(CapIdx pcb_cap, UBuffer &&child_cap_buf) {
        cap::Capability *cap = nullptr;
        auto pcb_res         = lookup_pcb(pcb_cap, &cap);
        propagate(pcb_res);
        cap::PCBObject obj(util::nnullforce(cap));
        auto target_res = obj.require_new_process();
        propagate(target_res);
        auto current_tcb_res = running_tcb();
        propagate(current_tcb_res);
        auto *current = current_tcb_res.value();
        if (current->task != target_res.value()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto holder_res = current_holder();
        propagate(holder_res);
        auto ret_slot_res = holder_res.value()->lookup_freeslot();
        propagate(ret_slot_res);
        CapIdx child_pcb_cap = ret_slot_res.value();
        auto *child_pcb_cap_out =
            reinterpret_cast<CapIdx *>(child_cap_buf.kbuf());
        CapIdx old_child_pcb_cap = *child_pcb_cap_out;
        *child_pcb_cap_out       = child_pcb_cap;
        auto restore_out_guard = util::Guard([&]() {
            memcpy(child_cap_buf.kbuf(), &old_child_pcb_cap,
                   sizeof(old_child_pcb_cap));
            auto commit_res = child_cap_buf.commit_to_user();
            assert(commit_res.has_value());
        });

        auto commit_res = child_cap_buf.commit_to_user();
        propagate(commit_res);

        auto fork_res = task::TaskManager::inst().fork_current(child_pcb_cap);
        propagate(fork_res);

        restore_out_guard.release();
        return fork_res.value().child_pid;
    }

    Result<bool> pcb_kill(CapIdx pcb_cap, int exit_code) {
        cap::Capability *cap = nullptr;
        auto pcb_res         = lookup_pcb(pcb_cap, &cap);
        propagate(pcb_res);
        cap::PCBObject obj(util::nnullforce(cap));
        auto kill_res = obj.kill(exit_code);
        propagate(kill_res);
        return true;
    }

    Result<bool> pcb_map(CapIdx pcb_cap, CapIdx mem_cap, size_t offset,
                         VirAddr vaddr, size_t sz, b64 protflg) {
        cap::Capability *pcb_cap_obj = nullptr;
        auto pcb_res                 = lookup_pcb(pcb_cap, &pcb_cap_obj);
        propagate(pcb_res);

        cap::Capability *mem_cap_obj = nullptr;
        cap::CHolder *target_holder  = pcb_res.value()->pcb->cholder;
        auto memory_res =
            lookup_memory_in_holder(target_holder, mem_cap, &mem_cap_obj);
        propagate(memory_res);
        cap::PCBObject pcb_obj(util::nnullforce(pcb_cap_obj));
        cap::MemoryObject mem_obj(util::nnullforce(mem_cap_obj));
        auto map_res = pcb_obj.map(mem_obj, offset, vaddr, sz, protflg);
        propagate(map_res);
        return true;
    }

    Result<bool> pcb_unmap(CapIdx pcb_cap, VirAddr vaddr, size_t sz) {
        cap::Capability *pcb_cap_obj = nullptr;
        auto pcb_res                 = lookup_pcb(pcb_cap, &pcb_cap_obj);
        propagate(pcb_res);
        cap::PCBObject pcb_obj(util::nnullforce(pcb_cap_obj));
        auto unmap_res = pcb_obj.unmap(vaddr, sz);
        propagate(unmap_res);
        return true;
    }

    Result<void> pcb_query_vaddr(CapIdx pcb_cap, VirAddr vaddr,
                                 UBuffer &&info_buf, bool expose_mem_cap) {
        cap::Capability *pcb_cap_obj = nullptr;
        auto pcb_res                 = lookup_pcb(pcb_cap, &pcb_cap_obj);
        propagate(pcb_res);
        cap::PCBObject pcb_obj(util::nnullforce(pcb_cap_obj));
        auto query_res = pcb_obj.query_vaddr(
            vaddr, expose_mem_cap ? cap::null : cap::error);
        propagate(query_res);
        if (info_buf.len() < sizeof(cap::VMAInfo)) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        memcpy(info_buf.kbuf(), &query_res.value(), sizeof(cap::VMAInfo));
        auto commit_res = info_buf.commit_to_user();
        propagate(commit_res);
        void_return();
    }

    Result<size_t> pcb_query_vspace(CapIdx pcb_cap, size_t offset,
                                    UBuffer &&info_buf, size_t max_entries,
                                    bool expose_mem_cap) {
        cap::Capability *pcb_cap_obj = nullptr;
        auto pcb_res                 = lookup_pcb(pcb_cap, &pcb_cap_obj);
        propagate(pcb_res);
        cap::PCBObject pcb_obj(util::nnullforce(pcb_cap_obj));
        auto query_res =
            pcb_obj.query_vspace(offset, max_entries, expose_mem_cap);
        propagate(query_res);
        if (info_buf.len() < query_res.value().size() * sizeof(cap::VMAInfo)) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        auto *out = reinterpret_cast<cap::VMAInfo *>(info_buf.kbuf());
        for (size_t i = 0; i < query_res.value().size(); ++i) {
            out[i] = query_res.value()[i];
        }
        auto commit_res = info_buf.commit_to_user();
        propagate(commit_res);
        return query_res.value().size();
    }

    Result<bool> pcb_execve(CapIdx pcb_cap, CapIdx image_cap,
                            const StartupArguments &startup) {
        cap::Capability *cap = nullptr;
        auto pcb_res         = lookup_pcb(pcb_cap, &cap);
        propagate(pcb_res);
        cap::PCBObject obj(util::nnullforce(cap));
        auto target_res = obj.require_execute();
        propagate(target_res);
        auto image_res = lookup_vfile(image_cap);
        propagate(image_res);

        auto exec_res = task::TaskManager::inst().exec_pcb(
            util::nnullforce(target_res.value()), image_cap,
            startup.caps.data(), startup.caps.size(), startup.argv,
            startup.envp, startup.bsargv);
        propagate(exec_res);
        return true;
    }

    bool pcb_is_current(CapIdx pcb_cap) {
        auto current_tcb_res = running_tcb();
        if (!current_tcb_res.has_value()) {
            return false;
        }
        auto pcb_res = lookup_pcb(pcb_cap, nullptr);
        if (!pcb_res.has_value()) {
            return false;
        }
        return current_tcb_res.value()->task == pcb_res.value()->pcb;
    }

    Result<size_t> get_pid(CapIdx pcb_cap) {
        auto holder_res = current_holder();
        propagate(holder_res);
        auto cap_res = holder_res.value()->lookup(pcb_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::PCB) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        auto pid_res =
            cap::PCBObject(util::nnullforce(cap_res.value())).get_pid();
        propagate(pid_res);
        return pid_res.value();
    }
}  // namespace syscall
