/**
 * @file process.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Process 资源绑定、提交事务与 kernel_proc 初始化。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <memory/virtual/user/vm.h>
#include <obj/process.h>
#include <tay/counter.h>

#include <limits>
#include <new>

namespace task {
    namespace {
        constinit tay::counter<u64_t> process_ids{1};
        ProcTable manager;
        cap::KObjectRef<Process> kernel_process_ref;
    }  // namespace

    Process::Process(bool kernel) noexcept
        : id_(kernel ? 0 : process_ids.next()), kernel_(kernel) {}

    tay::expected<cap::KObjectRef<Process>, ProcessError> Process::create() noexcept {
        auto *process = new (std::nothrow) Process(false);
        if (process == nullptr)
            return tay::Err(ProcessError::OutOfMemory());
        return cap::KObjectRef<Process>(*process);
    }

    tay::expected<cap::KObjectRef<Process>, ProcessError> Process::create_kernel() noexcept {
        auto *process = new (std::nothrow) Process(true);
        if (process == nullptr)
            return tay::Err(ProcessError::OutOfMemory());
        process->state_.lock()->lifecycle = ProcessState::SUBMITTED;
        return cap::KObjectRef<Process>(*process);
    }

    Process::~Process() noexcept {
        auto state = state_.lock();
        if (manager_hook_.in_list || !state->threads.empty())
            kernel::log::panic("销毁仍已发布或仍含 Thread 的 Process");
        state->lifecycle = ProcessState::DEAD;
    }

    tay::expected<void, ProcessError> Process::set_addr_space(AddrSpace &addr_space) noexcept {
        if (kernel_)
            return tay::Err(ProcessError::KernelProcessOperation());
        auto state = state_.lock();
        if (state->lifecycle != ProcessState::CREATED)
            return tay::Err(ProcessError::InvalidState(state->lifecycle));
        if (state->addr_space)
            return tay::Err(ProcessError::AddressSpaceAlreadySet());
        state->addr_space = cap::KObjectRef<AddrSpace>(addr_space);
        return {};
    }

    tay::expected<void, ProcessError> Process::set_cspace(cap::CSpace &cspace) noexcept {
        if (kernel_)
            return tay::Err(ProcessError::KernelProcessOperation());
        auto state = state_.lock();
        if (state->lifecycle != ProcessState::CREATED)
            return tay::Err(ProcessError::InvalidState(state->lifecycle));
        if (state->cspace)
            return tay::Err(ProcessError::CSpaceAlreadySet());
        state->cspace = cap::KObjectRef<cap::CSpace>(cspace);
        return {};
    }

    tay::expected<void, ProcessError> Process::submit() noexcept {
        return proc_table().submit(*this);
    }

    bool Process::attach_thread(Thread &thread) noexcept {
        auto state = state_.lock();
        if (state->lifecycle == ProcessState::STOPPING || state->lifecycle == ProcessState::DEAD ||
            state->threads.linked(&thread))
            return false;
        state->threads.push_back(&thread);
        return true;
    }

    void Process::detach_thread(Thread &thread) noexcept {
        auto state = state_.lock();
        if (state->threads.linked(&thread))
            (void)state->threads.remove(&thread);
    }

    void Process::activate_vm() noexcept {
        if (kernel_) {
            memory::activate_kernel_vm();
            return;
        }
        AddrSpace *addr_space = nullptr;
        {
            auto state = state_.lock();
            addr_space = state->addr_space.get();
        }
        if (addr_space == nullptr)
            kernel::log::panic("用户 Process 没有 AddrSpace");
        // 资源绑定仅允许 CREATED -> SUBMITTED 的单向转换；释放 Process 锁后才进入
        // AddrSpace，避免把 Process 同步域带入页表路径。
        addr_space->activate();
    }

    ProcessState Process::state() const noexcept {
        return state_.lock()->lifecycle;
    }

    bool Process::submitted() const noexcept {
        return state() == ProcessState::SUBMITTED;
    }

    AddrSpace *Process::addr_space() const noexcept {
        return state_.lock()->addr_space.get();
    }

    cap::CSpace *Process::cspace() const noexcept {
        return state_.lock()->cspace.get();
    }

    tay::expected<void, ProcessError> ProcTable::submit(Process &process) noexcept {
        if (process.kernel_)
            return tay::Err(ProcessError::KernelProcessOperation());
        auto manager_state = state_.lock();
        auto process_state = process.state_.lock();
        if (manager_state->processes.linked(&process) ||
            process_state->lifecycle == ProcessState::SUBMITTED)
            return tay::Err(ProcessError::AlreadySubmitted());
        if (process_state->lifecycle != ProcessState::CREATED)
            return tay::Err(ProcessError::InvalidState(process_state->lifecycle));
        if (!process_state->addr_space)
            return tay::Err(ProcessError::MissingAddressSpace());
        if (!process_state->cspace)
            return tay::Err(ProcessError::MissingCSpace());
        process.manager_ref_ = cap::KObjectRef<Process>(process);
        manager_state->processes.push_back(&process);
        process_state->lifecycle = ProcessState::SUBMITTED;
        return {};
    }

    size_t ProcTable::size() const noexcept {
        return state_.lock()->processes.size();
    }

    ProcTable &proc_table() noexcept {
        return manager;
    }

    tay::expected<void, ProcessError> init_kernel_proc() noexcept {
        if (kernel_process_ref)
            return tay::Err(ProcessError::AlreadySubmitted());
        kernel_process_ref = TAY_TRY(Process::create_kernel());
        return {};
    }

    Process &kernel_proc() noexcept {
        if (!kernel_process_ref)
            kernel::log::panic("kernel_proc 尚未初始化");
        return *kernel_process_ref;
    }
}  // namespace task
