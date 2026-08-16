/**
 * @file process.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Process 资源绑定、提交事务与 kernel_process 初始化。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <memory/virtual/client/client_space.h>
#include <obj/process.h>
#include <tay/counter.h>

#include <limits>
#include <new>

namespace task {
    namespace {
        constinit tay::counter<u64_t> process_ids{1};
        ProcessManager manager;
        cap::ObjectRef<Process> kernel_process_ref;
    }  // namespace

    Process::Process(bool kernel) noexcept
        : id_(kernel ? 0 : process_ids.next()), kernel_(kernel) {}

    tay::expected<cap::ObjectRef<Process>, ProcessError> Process::create() noexcept {
        auto *process = new (std::nothrow) Process(false);
        if (process == nullptr)
            return tay::Err(ProcessError::OutOfMemory());
        return cap::ObjectRef<Process>(*process);
    }

    tay::expected<cap::ObjectRef<Process>, ProcessError> Process::create_kernel() noexcept {
        auto *process = new (std::nothrow) Process(true);
        if (process == nullptr)
            return tay::Err(ProcessError::OutOfMemory());
        process->state_ = ProcessState::SUBMITTED;
        return cap::ObjectRef<Process>(*process);
    }

    Process::~Process() noexcept {
        if (manager_hook_.in_list || !threads_.empty())
            kernel::log::panic("销毁仍已发布或仍含 Thread 的 Process");
        state_ = ProcessState::DEAD;
    }

    tay::expected<void, ProcessError> Process::set_address_space(
        AddressSpace &address_space) noexcept {
        if (kernel_)
            return tay::Err(ProcessError::KernelProcessOperation());
        if (state_ != ProcessState::CREATED)
            return tay::Err(ProcessError::InvalidState(state_));
        if (address_space_)
            return tay::Err(ProcessError::AddressSpaceAlreadySet());
        address_space_ = cap::ObjectRef<AddressSpace>(address_space);
        return {};
    }

    tay::expected<void, ProcessError> Process::set_cspace(cap::CSpace &cspace) noexcept {
        if (kernel_)
            return tay::Err(ProcessError::KernelProcessOperation());
        if (state_ != ProcessState::CREATED)
            return tay::Err(ProcessError::InvalidState(state_));
        if (cspace_)
            return tay::Err(ProcessError::CSpaceAlreadySet());
        cspace_ = cap::ObjectRef<cap::CSpace>(cspace);
        return {};
    }

    tay::expected<void, ProcessError> Process::submit() noexcept {
        return process_manager().submit(*this);
    }

    bool Process::attach_thread(Thread &thread) noexcept {
        if (state_ == ProcessState::STOPPING || state_ == ProcessState::DEAD ||
            threads_.linked(&thread))
            return false;
        threads_.push_back(&thread);
        return true;
    }

    void Process::detach_thread(Thread &thread) noexcept {
        if (threads_.linked(&thread))
            (void)threads_.remove(&thread);
    }

    void Process::activate_address_space() noexcept {
        if (kernel_) {
            memory::activate_kernel_space();
            return;
        }
        if (!address_space_)
            kernel::log::panic("用户 Process 没有 AddressSpace");
        address_space_->activate();
    }

    tay::expected<void, ProcessError> ProcessManager::submit(Process &process) noexcept {
        if (process.kernel_)
            return tay::Err(ProcessError::KernelProcessOperation());
        if (processes_.linked(&process) || process.state_ == ProcessState::SUBMITTED)
            return tay::Err(ProcessError::AlreadySubmitted());
        if (process.state_ != ProcessState::CREATED)
            return tay::Err(ProcessError::InvalidState(process.state_));
        if (!process.address_space_)
            return tay::Err(ProcessError::MissingAddressSpace());
        if (!process.cspace_)
            return tay::Err(ProcessError::MissingCSpace());
        process.manager_ref_ = cap::ObjectRef<Process>(process);
        processes_.push_back(&process);
        process.state_ = ProcessState::SUBMITTED;
        return {};
    }

    ProcessManager &process_manager() noexcept {
        return manager;
    }

    tay::expected<void, ProcessError> initialize_kernel_process() noexcept {
        if (kernel_process_ref)
            return tay::Err(ProcessError::AlreadySubmitted());
        kernel_process_ref = TAY_TRY(Process::create_kernel());
        return {};
    }

    Process &kernel_process() noexcept {
        if (!kernel_process_ref)
            kernel::log::panic("kernel_process 尚未初始化");
        return *kernel_process_ref;
    }
}  // namespace task
