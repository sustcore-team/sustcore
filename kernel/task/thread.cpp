/**
 * @file thread.cpp
 * @brief 内核 Thread 的可失败构造与栈资源管理
 */

#include <memory/slab/heap.h>
#include <task/thread.h>

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
            memory::heap_allocator().deallocate(base_);
        base_  = nullptr;
        bytes_ = 0;
    }

    Thread Thread::adopt_current() noexcept {
        return Thread(adopt_current_tag{});
    }

    tay::expected<tay::unique_ptr<Thread>, tay::error_code> Thread::create_kernel(
        ThreadEntry entry, void *argument) noexcept {
        if (entry == nullptr)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        auto stack = KernelStack::create();
        if (!stack)
            return tay::Err(stack.error());
        auto *thread = new (std::nothrow) Thread(std::move(*stack), entry, argument);
        if (thread == nullptr)
            return tay::Err(tay::error_code::OUT_OF_MEMORY);
        return tay::unique_ptr<Thread>(thread);
    }

    Thread::Thread(KernelStack &&stack, ThreadEntry entry, void *argument) noexcept
        : stack_(std::move(stack)), entry_(entry), argument_(argument) {
        context_.ra() = reinterpret_cast<addr_t>(&scheduler::scheduler_thread_bootstrap);
        context_.sp() = stack_.top();
    }

    Thread::~Thread() noexcept {
        if (rq_hook_.in_list)
            tay::panic("destroying a linked Thread");
        if (state_ == ThreadState::RUNNING)
            tay::panic("destroying the running Thread");
    }
}  // namespace task
