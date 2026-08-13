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
#include <obj/thread.h>
#include <tay/counter.h>

#include <limits>
#include <new>

namespace task {
    namespace {
        constinit tay::counter<u64_t> process_ids{1};
        ProcessManager manager;
        cap::ObjectRef<Process> kernel_process_ref;
    }  // namespace

    ProcessThreadHookLocator::Hook &ProcessThreadHookLocator::operator()(
        Thread &thread) const noexcept {
        return thread.process_hook_;
    }

    const ProcessThreadHookLocator::Hook &ProcessThreadHookLocator::operator()(
        const Thread &thread) const noexcept {
        return thread.process_hook_;
    }

    Process::Process(bool kernel) noexcept : kernel_(kernel) {
        id_ = kernel ? 0 : process_ids.next();
    }

    tay::expected<cap::ObjectRef<Process>, tay::error_code> Process::create() noexcept {
        auto *process = new (std::nothrow) Process(false);
        if (process == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        return cap::ObjectRef<Process>(*process);
    }

    tay::expected<cap::ObjectRef<Process>, tay::error_code> Process::create_kernel() noexcept {
        auto *process = new (std::nothrow) Process(true);
        if (process == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        process->state_ = ProcessState::SUBMITTED;
        return cap::ObjectRef<Process>(*process);
    }

    Process::~Process() noexcept {
        if (manager_hook_.in_list || !threads_.empty())
            kernel::log::panic("销毁仍已发布或仍含 Thread 的 Process");
        state_ = ProcessState::DEAD;
    }

    tay::expected<void, tay::error_code> Process::set_address_space(
        AddressSpace &address_space) noexcept {
        if (kernel_ || state_ != ProcessState::CREATED)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        address_space_ = cap::ObjectRef<AddressSpace>(address_space);
        return {};
    }

    tay::expected<void, tay::error_code> Process::set_cspace(cap::CSpace &cspace) noexcept {
        if (kernel_ || state_ != ProcessState::CREATED)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        cspace_ = cap::ObjectRef<cap::CSpace>(cspace);
        return {};
    }

    tay::expected<void, tay::error_code> Process::submit() noexcept {
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

    tay::expected<void, tay::error_code> ProcessManager::submit(Process &process) noexcept {
        if (process.kernel_ || process.state_ != ProcessState::CREATED || !process.address_space_ ||
            !process.cspace_ || processes_.linked(&process))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        process.manager_ref_ = cap::ObjectRef<Process>(process);
        processes_.push_back(&process);
        process.state_ = ProcessState::SUBMITTED;
        return {};
    }

    ProcessManager &process_manager() noexcept {
        return manager;
    }

    tay::expected<void, tay::error_code> initialize_kernel_process() noexcept {
        if (kernel_process_ref)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        auto process = Process::create_kernel();
        if (!process)
            return tay::Err(process.error());
        kernel_process_ref = std::move(*process);
        return {};
    }

    Process &kernel_process() noexcept {
        if (!kernel_process_ref)
            kernel::log::panic("kernel_process 尚未初始化");
        return *kernel_process_ref;
    }
}  // namespace task
