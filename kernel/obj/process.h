/**
 * @file process.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Process 资源域、提交状态与全局 ProcessManager。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <obj/address_space.h>
#include <obj/cspace.h>
#include <obj/kernel_object.h>
#include <obj/objfwd.h>
#include <tay/expected.h>
#include <tay/list.h>

namespace task {
    enum class ProcessState : u8_t {
        CREATED,
        SUBMITTED,
        STOPPING,
        DEAD,
    };

    struct ProcessThreadHookLocator {
        using Hook = tay::intrusive_list_hook<Thread *, Thread *>;
        [[nodiscard]] Hook &operator()(Thread &thread) const noexcept;
        [[nodiscard]] const Hook &operator()(const Thread &thread) const noexcept;
    };

    class ProcessManager;
    struct ProcessManagerHookLocator;

    class Process final : public cap::TypedKernelObject<Process, cap::ObjectType::PROCESS> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::PROCESS;

        [[nodiscard]] static tay::expected<cap::ObjectRef<Process>, tay::error_code>
        create() noexcept;
        [[nodiscard]] static tay::expected<cap::ObjectRef<Process>, tay::error_code>
        create_kernel() noexcept;

        Process(const Process &)            = delete;
        Process &operator=(const Process &) = delete;
        Process(Process &&)                 = delete;
        Process &operator=(Process &&)      = delete;
        ~Process() noexcept;

        [[nodiscard]] tay::expected<void, tay::error_code> set_address_space(
            AddressSpace &address_space) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> set_cspace(cap::CSpace &cspace) noexcept;
        [[nodiscard]] tay::expected<void, tay::error_code> submit() noexcept;

        [[nodiscard]] u64_t id() const noexcept {
            return id_;
        }
        [[nodiscard]] ProcessState state() const noexcept {
            return state_;
        }
        [[nodiscard]] bool submitted() const noexcept {
            return state_ == ProcessState::SUBMITTED;
        }
        [[nodiscard]] bool kernel() const noexcept {
            return kernel_;
        }
        [[nodiscard]] AddressSpace *address_space() const noexcept {
            return address_space_.get();
        }
        [[nodiscard]] cap::CSpace *cspace() const noexcept {
            return cspace_.get();
        }

        void activate_address_space() noexcept;

    private:
        friend class ProcessManager;
        friend class Thread;
        friend struct ProcessManagerHookLocator;

        explicit Process(bool kernel) noexcept;
        [[nodiscard]] bool attach_thread(Thread &thread) noexcept;
        void detach_thread(Thread &thread) noexcept;

        using manager_hook = tay::intrusive_list_hook<Process *, Process *>;
        using thread_list  = tay::intrusive_list<Thread, ProcessThreadHookLocator>;

        cap::ObjectRef<AddressSpace> address_space_{};
        cap::ObjectRef<cap::CSpace> cspace_{};
        cap::ObjectRef<Process> manager_ref_{};
        thread_list threads_{};
        manager_hook manager_hook_{};
        u64_t id_           = 0;
        ProcessState state_ = ProcessState::CREATED;
        bool kernel_        = false;
    };

    class ProcessManager final {
    public:
        [[nodiscard]] tay::expected<void, tay::error_code> submit(Process &process) noexcept;
        [[nodiscard]] size_t size() const noexcept {
            return processes_.size();
        }

    private:
        using process_list = tay::intrusive_list<
            Process, tay::locate_member<Process, Process::manager_hook, &Process::manager_hook_>>;
        process_list processes_{};
    };

    [[nodiscard]] ProcessManager &process_manager() noexcept;
    [[nodiscard]] tay::expected<void, tay::error_code> initialize_kernel_process() noexcept;
    [[nodiscard]] Process &kernel_process() noexcept;
}  // namespace task
