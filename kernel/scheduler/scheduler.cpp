/**
 * @file scheduler.cpp
 * @brief 单核 Thread 状态迁移与协作式上下文切换
 */

#include <arch/interrupt.h>
#include <log.h>
#include <obj/process.h>
#include <scheduler/scheduler.h>

#include <utility>

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
        if (!ready_ || thread.scheduler_attached_ || !thread.configured_ ||
            (thread.state_ != task::ThreadState::CREATED &&
             thread.state_ != task::ThreadState::SUSPENDED) ||
            !thread.process_->submitted())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        thread.state_              = task::ThreadState::READY;
        thread.scheduler_attached_ = true;
        thread.scheduler_ref_      = cap::ObjectRef<task::Thread>(thread);
        if (!rq_.push(&thread)) {
            thread.state_              = task::ThreadState::SUSPENDED;
            thread.scheduler_attached_ = false;
            thread.scheduler_ref_.reset();
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        }
        return {};
    }

    tay::expected<void, tay::error_code> Scheduler::suspend(task::Thread &thread) noexcept {
        hal::interrupt_guard guard;
        if (!ready_ || !thread.scheduler_attached_ || thread.state_ != task::ThreadState::READY ||
            !rq_.remove(&thread))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        thread.state_              = task::ThreadState::SUSPENDED;
        thread.scheduler_attached_ = false;
        thread.scheduler_ref_.reset();
        return {};
    }

    task::Thread *Scheduler::pick_next() noexcept {
        auto *next = rq_.pop();
        if (next != nullptr && next->state_ != task::ThreadState::READY)
            kernel::log::panic("FIFO run queue contains a non-ready Thread");
        return next;
    }

    void Scheduler::switch_to(task::Thread &previous, task::Thread &next) noexcept {
        next.process_->activate_address_space();
        current_    = &next;
        next.state_ = task::ThreadState::RUNNING;
        if (&previous == &next)
            return;
        hal::__switch_to(&previous.context_, &next.context_);
        // 退出线程把最后一个 scheduler 引用交给下一条可运行线程；此处已经运行在
        // 恢复线程的栈上，可以安全销毁退出线程的 TCB 与内核栈。
        deferred_exit_.reset();
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

        auto *previous                = current_;
        previous->state_              = task::ThreadState::EXITED;
        previous->scheduler_attached_ = false;
        if (deferred_exit_)
            kernel::log::panic("存在尚未回收的退出 Thread");
        deferred_exit_ = std::move(previous->scheduler_ref_);
        auto *next     = pick_next();
        if (next == nullptr)
            kernel::log::panic("last runnable Thread attempted to exit");
        switch_to(*previous, *next);
        kernel::log::panic("exited Thread resumed");
    }

    [[noreturn]] void Scheduler::bootstrap_current() noexcept {
        if (current_ == nullptr)
            kernel::log::panic("invalid Thread bootstrap");
        // 新线程没有从另一条 switch_to 调用链返回，必须在 bootstrap 栈上完成前任回收。
        deferred_exit_.reset();
        if (current_->mode_ == task::ThreadMode::USER) {
            hal::enter_user(current_->user_frame_, current_->stack_.top());
        }
        if (current_->entry_ == nullptr)
            kernel::log::panic("invalid kernel Thread bootstrap");
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
