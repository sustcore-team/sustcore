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
#include <logger.h>
#include <object/task.h>
#include <schd/schdbase.h>
#include <sus/nonnull.h>
#include <sus/raii.h>
#include <sustcore/capability.h>
#include <syscall/task.h>
#include <syscall/uaccess.h>
#include <task/task.h>

namespace syscall {
    CapIdx create_process(const UString &path, VirAddr caps_uaddr,
                          size_t caps_sz) {
        // 1) 获取当前 CSpace 作为能力来源
        auto current_holder_res = cap::CHolder::current();
        if (!current_holder_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: 当前CSpace不可用");
            return cap::error;
        }
        cap::CHolder *current_holder = current_holder_res.value();

        // 2) 加载子进程 ELF，并设置失败清理守卫
        auto load_res =
            task::TaskManager::inst().load_elf(path.kbuf(), schd::ClassType::RR);
        if (!load_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: path=%s, 错误码: %s",
                                    path.kbuf(), to_cstring(load_res.error()));
            return cap::error;
        }
        auto pcb_guard = util::Guard([&]() {
            if (load_res.has_value()) {
                auto pcb = load_res.value();
                loggers::SYSCALL::INFO("清理已加载的PCB对象: pid=%d", pcb->pid);
                // TODO: 调用 task manager 的接口来删除 PCB 对象
                // task::TaskManager::inst().remove_pcb(pcb->pid);
            }
        });

        auto pcb = load_res.value();
        auto child_pcb_cap_res = pcb->cholder->internal_lookup(pcb->pcb_cap);
        if (!child_pcb_cap_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: 子进程PCB能力不可用 err=%d",
                                    child_pcb_cap_res.error());
            return cap::error;
        }

        cap::PCBObject child_pcb_obj(
            util::nnullforce(child_pcb_cap_res.value()));

        // 3) 将调用方传入的初始能力集合注入子进程
        if (caps_sz != 0) {
            UBuffer caps_buf(caps_uaddr, caps_sz * sizeof(CapIdx));
            caps_buf.sync_from_user();
            auto *caps = reinterpret_cast<CapIdx *>(caps_buf.kbuf());
            for (size_t i = 0; i < caps_sz; ++i) {
                auto insert_res =
                    child_pcb_obj.cap_insert(caps[i], *current_holder);
                if (!insert_res.has_value()) {
                    loggers::SYSCALL::ERROR(
                        "创建进程失败: 初始能力插入失败 idx=%d err=%d", i,
                        insert_res.error());
                    return cap::error;
                }
            }
        }

        // 4) 返回子进程 PCB 能力给调用方
        auto ret_slot_res = current_holder->internal_lookup_freeslot();
        if (!ret_slot_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: 调用者无空闲能力槽 err=%d",
                                    ret_slot_res.error());
            return cap::error;
        }
        auto ret_insert_res = current_holder->internal_insert(
            ret_slot_res.value(), child_pcb_cap_res.value()->payload(),
            child_pcb_cap_res.value()->perm());
        if (!ret_insert_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: 返回PCB能力插入失败 err=%d",
                                    ret_insert_res.error());
            return cap::error;
        }

        loggers::SYSCALL::INFO("创建进程成功: path=%s, pid=%d", path.kbuf(),
                               pcb->pid);
        pcb_guard.release();
        return ret_slot_res.value();
    }

    ForkRet fork() {
        auto fork_res = task::TaskManager::inst().fork_current();
        if (!fork_res.has_value()) {
            loggers::SYSCALL::ERROR("fork失败: err=%d", fork_res.error());
            return {cap::error, 0};
        }
        return {fork_res.value().child_pcb_cap, fork_res.value().child_pid};
    }

    void exit() {
        auto exit_res = task::TaskManager::inst().exit_current();
        if (!exit_res.has_value()) {
            loggers::SYSCALL::ERROR("exit失败: err=%d", exit_res.error());
        }
    }

    size_t get_pid(CapIdx pcb_cap) {
        auto cap_res = cap::CHolder::lookup(pcb_cap);
        if (!cap_res.has_value()) {
            loggers::SYSCALL::ERROR("get_pid失败: err=%d", cap_res.error());
            return 0;
        }
        if (cap_res.value()->payload()->type_id() != PayloadType::PCB) {
            loggers::SYSCALL::ERROR("get_pid失败: 类型不匹配");
            return 0;
        }

        auto pid_res =
            cap::PCBObject(util::nnullforce(cap_res.value())).get_pid();
        if (!pid_res.has_value()) {
            loggers::SYSCALL::ERROR("get_pid失败: err=%d", pid_res.error());
            return 0;
        }
        return pid_res.value();
    }
}  // namespace syscall
