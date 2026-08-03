/**
 * @file scheduler.cpp
 * @brief 单核 Thread 状态迁移与协作式上下文切换
 */

#include <arch/interrupt.h>
#include <log.h>
#include <scheduler/scheduler.h>

namespace scheduler {
    namespace {
        Scheduler scheduler;
    }

    Scheduler &instance() noexcept {
        return scheduler;
    }

    tay::expected<void, tay::error_code> Scheduler::initialize(task::Thread &bootstrap) noexcept {
        if (ready_ || current_ != nullptr || bootstrap.state_ != task::ThreadState::RUNNING)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        if (hal::interrupts_enabled())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        current_ = &bootstrap;
        ready_   = true;
        return {};
    }

    tay::expected<void, tay::error_code> Scheduler::resume(task::Thread &thread) noexcept {
        hal::interrupt_guard guard;
        if (!ready_ || thread.state_ != task::ThreadState::SUSPENDED)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        thread.state_ = task::ThreadState::READY;
        if (!rq_.push(&thread)) {
            thread.state_ = task::ThreadState::SUSPENDED;
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        return {};
    }

    task::Thread *Scheduler::pick_next() noexcept {
        auto *next = rq_.pop();
        if (next != nullptr && next->state_ != task::ThreadState::READY)
            kernel::log::panic("FIFO run queue contains a non-ready Thread");
        return next;
    }

    void Scheduler::switch_to(task::Thread &previous, task::Thread &next) noexcept {
        current_    = &next;
        next.state_ = task::ThreadState::RUNNING;
        if (&previous == &next)
            return;
        hal::__switch_to(&previous.context_, &next.context_);
        if (current_ != &previous || previous.state_ != task::ThreadState::RUNNING)
            kernel::log::panic("Thread resumed with inconsistent scheduler state");
    }

    void Scheduler::yield() noexcept {
        hal::interrupt_guard guard;
        if (!ready_ || current_ == nullptr || current_->state_ != task::ThreadState::RUNNING)
            kernel::log::panic("invalid scheduler yield");
        if (rq_.empty())
            return;

        auto *previous   = current_;
        previous->state_ = task::ThreadState::READY;
        if (!rq_.push(previous))
            kernel::log::panic("running Thread is already linked in the FIFO queue");
        auto *next = pick_next();
        if (next == nullptr)
            kernel::log::panic("FIFO run queue lost the next Thread");
        switch_to(*previous, *next);
    }

    [[noreturn]] void Scheduler::exit_current() noexcept {
        hal::disable_interrupts();
        if (!ready_ || current_ == nullptr || current_->state_ != task::ThreadState::RUNNING)
            kernel::log::panic("invalid Thread exit");

        auto *previous   = current_;
        previous->state_ = task::ThreadState::EXITED;
        auto *next       = pick_next();
        if (next == nullptr)
            kernel::log::panic("last runnable Thread attempted to exit");
        switch_to(*previous, *next);
        kernel::log::panic("exited Thread resumed");
    }

    [[noreturn]] void Scheduler::bootstrap_current() noexcept {
        if (current_ == nullptr || current_->entry_ == nullptr)
            kernel::log::panic("invalid Thread bootstrap");
        current_->entry_(current_->argument_);
        exit_current();
    }

    void yield() noexcept {
        instance().yield();
    }

    [[noreturn]] void exit_current() noexcept {
        instance().exit_current();
    }

    extern "C" [[noreturn]] void scheduler_thread_bootstrap() noexcept {
        instance().bootstrap_current();
    }
}  // namespace scheduler
