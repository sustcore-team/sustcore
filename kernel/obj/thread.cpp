/**
 * @file thread.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Thread 构造、Process 绑定与用户上下文配置。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <memory/slab/heap.h>
#include <obj/process.h>
#include <obj/thread.h>

#include <new>
#include <utility>

namespace scheduler {
    extern "C" [[noreturn]] void scheduler_thread_bootstrap() noexcept;
}

namespace task {
    tay::expected<KernelStack, tay::error_code> KernelStack::create(size_t bytes) noexcept {
        if (bytes == 0 || bytes % PAGE_SIZE != 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        auto *allocator = memory::try_heap_allocator();
        if (allocator == nullptr)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        auto storage = allocator->try_allocate(bytes, PAGE_SIZE);
        if (!storage)
            return tay::Err(storage.error());
        return KernelStack(static_cast<std::byte *>(*storage), bytes);
    }

    KernelStack::KernelStack(KernelStack &&other) noexcept
        : base_(other.base_), bytes_(other.bytes_) {
        other.base_  = nullptr;
        other.bytes_ = 0;
    }

    KernelStack &KernelStack::operator=(KernelStack &&other) noexcept {
        if (this == &other)
            return *this;
        reset();
        base_        = other.base_;
        bytes_       = other.bytes_;
        other.base_  = nullptr;
        other.bytes_ = 0;
        return *this;
    }

    KernelStack::~KernelStack() noexcept {
        reset();
    }

    addr_t KernelStack::top() const noexcept {
        const auto end = reinterpret_cast<addr_t>(base_ + bytes_);
        return end & ~static_cast<addr_t>(hal::CONTEXT_ALIGNMENT - 1);
    }

    void KernelStack::reset() noexcept {
        if (base_ != nullptr)
            memory::dealloc(base_);
        base_  = nullptr;
        bytes_ = 0;
    }

    Thread::Thread(Process &process, adopt_current_tag) noexcept
        : process_(process),
          state_(ThreadState::RUNNING),
          configured_(true),
          scheduler_attached_(true) {
        (void)process.attach_thread(*this);
    }

    Thread Thread::adopt_current(Process &process) noexcept {
        return Thread(process, adopt_current_tag{});
    }

    Thread::Thread(Process &process, KernelStack &&stack, ThreadMode mode, ThreadEntry entry,
                   void *argument) noexcept
        : process_(process),
          stack_(std::move(stack)),
          entry_(entry),
          argument_(argument),
          mode_(mode),
          configured_(mode == ThreadMode::KERNEL) {
        context_.ra() = reinterpret_cast<addr_t>(&scheduler::scheduler_thread_bootstrap);
        context_.sp() = stack_.top();
        (void)process.attach_thread(*this);
    }

    tay::expected<cap::ObjectRef<Thread>, tay::error_code> Thread::create_kernel(
        Process &process, ThreadEntry entry, void *argument) noexcept {
        if (!process.kernel() || entry == nullptr)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        auto stack = KernelStack::create();
        if (!stack)
            return tay::Err(stack.error());
        auto *thread = new (std::nothrow)
            Thread(process, std::move(*stack), ThreadMode::KERNEL, entry, argument);
        if (thread == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        return cap::ObjectRef<Thread>(*thread);
    }

    tay::expected<cap::ObjectRef<Thread>, tay::error_code> Thread::create_user(
        Process &process) noexcept {
        if (process.kernel() || process.state() == ProcessState::STOPPING ||
            process.state() == ProcessState::DEAD)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        auto stack = KernelStack::create();
        if (!stack)
            return tay::Err(stack.error());
        auto *thread = new (std::nothrow)
            Thread(process, std::move(*stack), ThreadMode::USER, nullptr, nullptr);
        if (thread == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        return cap::ObjectRef<Thread>(*thread);
    }

    tay::expected<void, tay::error_code> Thread::configure_user(addr_t entry, addr_t stack_pointer,
                                                                addr_t argument) noexcept {
        if (mode_ != ThreadMode::USER || state_ != ThreadState::CREATED || configured_ ||
            entry == 0 || stack_pointer == 0 || (stack_pointer & 0xf) != 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        hal::initialize_user_frame(user_frame_, entry, stack_pointer, argument);
        configured_ = true;
        return {};
    }

    Thread::~Thread() noexcept {
        if (rq_hook_.in_list || scheduler_attached_ || state_ == ThreadState::RUNNING)
            tay::panic("destroying a scheduled Thread");
        process_->detach_thread(*this);
    }
}  // namespace task
