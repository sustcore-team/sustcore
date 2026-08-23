/**
 * @file process.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Process 资源域、提交状态与全局 ProcTable。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <obj/addr_space.h>
#include <obj/cspace.h>
#include <obj/kobject.h>
#include <obj/objfwd.h>
#include <obj/thread.h>
#include <synchronized.h>
#include <task/process_error.h>
#include <task/state.h>
#include <tay/expected.h>
#include <tay/list.h>

namespace task {
    class ProcTable;

    class Process final : public cap::TypedKObject<Process, cap::ObjectType::PROCESS> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::PROCESS;

        [[nodiscard]] static tay::expected<cap::KObjectRef<Process>, ProcessError>
        create() noexcept;
        [[nodiscard]] static tay::expected<cap::KObjectRef<Process>, ProcessError>
        create_kernel() noexcept;

        Process(const Process &)            = delete;
        Process &operator=(const Process &) = delete;
        Process(Process &&)                 = delete;
        Process &operator=(Process &&)      = delete;
        ~Process() noexcept;

        [[nodiscard]] tay::expected<void, ProcessError> set_addr_space(
            AddrSpace &addr_space) noexcept;
        [[nodiscard]] tay::expected<void, ProcessError> set_cspace(cap::CSpace &cspace) noexcept;
        [[nodiscard]] tay::expected<void, ProcessError> submit() noexcept;

        [[nodiscard]] u64_t id() const noexcept {
            return id_;
        }
        [[nodiscard]] ProcessState state() const noexcept;
        [[nodiscard]] bool submitted() const noexcept;
        [[nodiscard]] bool kernel() const noexcept {
            return kernel_;
        }
        [[nodiscard]] AddrSpace *addr_space() const noexcept;
        [[nodiscard]] cap::CSpace *cspace() const noexcept;

        void activate_vm() noexcept;

    private:
        friend class ProcTable;
        friend class Thread;

        explicit Process(bool kernel) noexcept;
        [[nodiscard]] bool attach_thread(Thread &thread) noexcept;
        void detach_thread(Thread &thread) noexcept;

        using thread_list = tay::intrusive_list<
            Thread, tay::locate_member<Thread, Thread::process_hook, &Thread::process_hook_>>;

        /**
         * @brief Process 生命周期、资源绑定和 Thread 链表的唯一同步域。
         * @note Process lock 不得持有到 scheduler attach/wake；调用方先完成对象状态提交。
         */
        struct State final {
            cap::KObjectRef<AddrSpace> addr_space{};
            cap::KObjectRef<cap::CSpace> cspace{};
            thread_list threads{};
            ProcessState lifecycle = ProcessState::CREATED;
        };

        cap::KObjectRef<Process> manager_ref_{};
        using manager_hook = tay::intrusive_list_hook<Process *, Process *>;
        manager_hook manager_hook_{};
        kernel::synchronized<State> state_{};
        u64_t id_    = 0;
        bool kernel_ = false;
    };

    class ProcTable final {
    public:
        [[nodiscard]] tay::expected<void, ProcessError> submit(Process &process) noexcept;
        [[nodiscard]] size_t size() const noexcept;

    private:
        using process_list = tay::intrusive_list<
            Process, tay::locate_member<Process, Process::manager_hook, &Process::manager_hook_>>;
        struct State final {
            process_list processes{};
        };
        kernel::synchronized<State> state_{};
    };

    [[nodiscard]] ProcTable &proc_table() noexcept;
    [[nodiscard]] tay::expected<void, ProcessError> init_kernel_proc() noexcept;
    [[nodiscard]] Process &kernel_proc() noexcept;
}  // namespace task
