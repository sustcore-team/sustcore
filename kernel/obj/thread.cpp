/**
 * @file thread.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Thread 构造、Process 绑定、用户上下文与 timed wait 生命周期。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <async/worklet.h>
#include <log.h>
#include <memory/slab/heap.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/adapter.h>
#include <scheduler/scheduler.h>
#include <tay/lock.h>
#include <timer/timer_engine.h>

#include <new>
#include <utility>

namespace scheduler {
    extern "C" [[noreturn]] void scheduler_thread_bootstrap() noexcept;
}

namespace task {
    tay::expected<KernelStack, ThreadError> KernelStack::create(size_t bytes) noexcept {
        if (bytes == 0 || bytes % PAGE_SIZE != 0)
            return tay::Err(ThreadError::InvalidStackSize(bytes));
        auto *allocator = memory::try_heap_allocator();
        if (allocator == nullptr)
            return tay::Err(ThreadError::HeapUnavailable());
        auto storage = allocator->try_allocate(bytes, PAGE_SIZE);
        if (!storage)
            return tay::Err(ThreadError::OutOfMemory());
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
        scheduler::ThreadSchedAdapter::initialize(*this);
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
        scheduler::ThreadSchedAdapter::initialize(*this);
        context_.ra() = reinterpret_cast<addr_t>(&scheduler::scheduler_thread_bootstrap);
        context_.sp() = stack_.top();
        (void)process.attach_thread(*this);
    }

    tay::expected<cap::ObjectRef<Thread>, ThreadError> Thread::create_kernel(
        Process &process, ThreadEntry entry, void *argument) noexcept {
        if (!process.kernel())
            return tay::Err(ThreadError::InvalidMode(ThreadMode::KERNEL));
        if (process.state() != ProcessState::SUBMITTED)
            return tay::Err(ThreadError::InvalidProcessState(process.state()));
        if (entry == nullptr)
            return tay::Err(ThreadError::InvalidEntry(0));
        auto stack   = TAY_TRY(KernelStack::create());
        auto *thread = new (std::nothrow)
            Thread(process, std::move(stack), ThreadMode::KERNEL, entry, argument);
        if (thread == nullptr)
            return tay::Err(ThreadError::OutOfMemory());
        return cap::ObjectRef<Thread>(*thread);
    }

    tay::expected<cap::ObjectRef<Thread>, ThreadError> Thread::create_user(
        Process &process) noexcept {
        if (process.kernel())
            return tay::Err(ThreadError::InvalidMode(ThreadMode::USER));
        if (process.state() == ProcessState::STOPPING || process.state() == ProcessState::DEAD)
            return tay::Err(ThreadError::InvalidProcessState(process.state()));
        auto stack   = TAY_TRY(KernelStack::create());
        auto *thread = new (std::nothrow)
            Thread(process, std::move(stack), ThreadMode::USER, nullptr, nullptr);
        if (thread == nullptr)
            return tay::Err(ThreadError::OutOfMemory());
        return cap::ObjectRef<Thread>(*thread);
    }

    tay::expected<void, ThreadError> Thread::configure_user(addr_t entry, addr_t stack_pointer,
                                                            addr_t argument) noexcept {
        if (mode_ != ThreadMode::USER)
            return tay::Err(ThreadError::InvalidMode(mode_));
        if (scheduler_attached_)
            return tay::Err(ThreadError::SchedulerAttached());
        if (state_ != ThreadState::CREATED || configured_)
            return tay::Err(ThreadError::AlreadyConfigured());
        if (entry == 0)
            return tay::Err(ThreadError::InvalidEntry(entry));
        if (stack_pointer == 0 || (stack_pointer & 0xf) != 0)
            return tay::Err(ThreadError::InvalidUserStack(stack_pointer));
        hal::initialize_user_frame(user_frame_, entry, stack_pointer, argument);
        configured_ = true;
        return {};
    }

    void Thread::TimedWaitWorklet::setup(Thread &thread, u64_t generation) noexcept {
        if (generation == 0)
            kernel::log::panic("configuring a timed-wait Worklet with an invalid generation");
        if (pending())
            kernel::log::panic("configuring a pending timed-wait Worklet");
        thread_     = &thread;
        generation_ = generation;
    }

    void Thread::TimedWaitWorklet::run() noexcept {
        auto *thread          = thread_;
        const auto generation = generation_;
        thread_               = nullptr;
        generation_           = 0;
        if (thread == nullptr || generation == 0)
            kernel::log::panic("timed-wait Worklet has no typed completion target");

        bool wake_waiter = false;
        {
            hal::interrupt_guard interrupt_guard;
            tay::lock_guard guard(thread->timed_wait_lock_);
            if (generation == thread->timed_wait_generation_ &&
                thread->timed_wait_state_ == TimedWaitState::PENDING)
            {
                thread->timed_wait_state_  = TimedWaitState::COMPLETED;
                thread->timed_wait_result_ = TimedWaitResult::TIMEOUT;
                wake_waiter                = true;
            }
        }

        auto &engine = kernel::timer::bsp_timer_engine();
        engine.retire(thread->timed_wait_timer_);
        engine.reset(thread->timed_wait_timer_);

        // 本地中断保持关闭直到 unpin，避免 BSP waiter 在 bookkeeping 清零与实际释放 pin
        // 之间重新运行；未来跨 CPU waiter 仍需独立的 owner 生命周期协议。
        hal::interrupt_guard interrupt_guard;
        bool release_pin = false;
        {
            tay::lock_guard guard(thread->timed_wait_lock_);
            if (thread->timed_wait_pin_active_ && thread->timed_wait_pin_generation_ == generation)
            {
                thread->timed_wait_pin_active_     = false;
                thread->timed_wait_pin_generation_ = 0;
                release_pin                        = true;
            }
        }
        if (!release_pin)
            kernel::log::panic("timed-wait Worklet did not own exactly one Thread pin");

        // wake 可能让 waiter 退出并释放 scheduler 强引用；pin 在此之前覆盖整个 cleanup。
        if (wake_waiter)
            scheduler::instance().wake(*thread, scheduler::WakeReason::WAIT_COMPLETED);

        // 最后一个 pin 可以销毁 Thread（并连带销毁本 Worklet）；此后不得访问任何成员。
        thread->unpin();
    }

    tay::expected<TimedWaitResult, ThreadError> Thread::wait_until(units::time deadline) noexcept {
        auto &core = scheduler::instance();
        if (core.current() != this)
            return tay::Err(ThreadError::NotCurrentThread());
        if (state_ != ThreadState::RUNNING)
            return tay::Err(ThreadError::InvalidThreadState(state_));

        hal::interrupt_guard interrupt_guard;
        u64_t generation = 0;
        {
            tay::lock_guard guard(timed_wait_lock_);
            // 配置检查固定按 timed-wait -> timer engine 取锁；engine 路径不会反向取得本锁。
            if (timed_wait_state_ != TimedWaitState::IDLE ||
                timed_wait_result_ != TimedWaitResult::NONE || timed_wait_pin_active_ ||
                timed_wait_pin_generation_ != 0 ||
                kernel::timer::bsp_timer_engine().state(timed_wait_timer_) !=
                    kernel::timer::PrecisionTimerState::IDLE ||
                timed_wait_worklet_.pending())
                return tay::Err(ThreadError::TimedWaitActive());
            ++timed_wait_generation_;
            if (timed_wait_generation_ == 0)
                ++timed_wait_generation_;
            generation = timed_wait_generation_;
            if (!try_pin())
                return tay::Err(ThreadError::PinFailed());
            timed_wait_pin_active_     = true;
            timed_wait_pin_generation_ = generation;
            timed_wait_state_          = TimedWaitState::PENDING;
            timed_wait_result_         = TimedWaitResult::NONE;
            timed_wait_worklet_.setup(*this, generation);
        }

        auto armed =
            kernel::timer::bsp_timer_engine().arm(timed_wait_timer_, deadline, timed_wait_worklet_);
        if (!armed) {
            {
                tay::lock_guard guard(timed_wait_lock_);
                timed_wait_state_          = TimedWaitState::IDLE;
                timed_wait_pin_active_     = false;
                timed_wait_pin_generation_ = 0;
            }
            unpin();
            return tay::Err(ThreadError::TimerArmFailed(armed.error().code()));
        }

        auto token = core.prepare_block_current();
        if (!token)
            kernel::log::panic("timed wait failed to prepare its two-phase park");
        if (auto committed = core.commit_block_current(std::move(*token)); !committed)
            kernel::log::panic("timed wait failed to commit its two-phase park");

        // 显式 winner 可以先唤醒 Thread；在 completion retire/reset 并清 pin 前禁止复用节点。
        for (;;) {
            {
                tay::lock_guard guard(timed_wait_lock_);
                if (timed_wait_state_ == TimedWaitState::COMPLETED && !timed_wait_pin_active_)
                    break;
            }
            scheduler::yield();
        }

        TimedWaitResult result;
        {
            tay::lock_guard guard(timed_wait_lock_);
            result             = timed_wait_result_;
            timed_wait_state_  = TimedWaitState::IDLE;
            timed_wait_result_ = TimedWaitResult::NONE;
        }
        return result;
    }

    bool Thread::finish_timed_wait(u64_t generation, TimedWaitResult result) noexcept {
        {
            hal::interrupt_guard interrupt_guard;
            tay::lock_guard guard(timed_wait_lock_);
            if (generation == 0 || generation != timed_wait_generation_ ||
                timed_wait_state_ != TimedWaitState::PENDING)
                return false;
            timed_wait_state_  = TimedWaitState::COMPLETED;
            timed_wait_result_ = result;
        }

        // completion 可能已经摘下 Worklet 并开始 retire；NOT_ARMED 表示 cleanup 已越过 cancel 点。
        (void)kernel::timer::bsp_timer_engine().cancel(timed_wait_timer_);
        scheduler::instance().wake(*this, scheduler::WakeReason::WAIT_COMPLETED);
        return true;
    }

    bool Thread::wake_timed_wait(u64_t generation) noexcept {
        return finish_timed_wait(generation, TimedWaitResult::WOKEN);
    }

    bool Thread::cancel_timed_wait(u64_t generation) noexcept {
        return finish_timed_wait(generation, TimedWaitResult::CANCELLED);
    }

    u64_t Thread::timed_wait_generation() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(timed_wait_lock_);
        return timed_wait_state_ == TimedWaitState::PENDING ? timed_wait_generation_ : 0;
    }

    bool Thread::timed_wait_idle() noexcept {
        hal::interrupt_guard interrupt_guard;
        tay::lock_guard guard(timed_wait_lock_);
        // scheduler detach 可持 run queue lock 调用本函数，固定锁序为 scheduler -> timed-wait
        // -> timer engine；timer completion 分段释放各锁后才调用 scheduler。
        return timed_wait_state_ == TimedWaitState::IDLE &&
               timed_wait_result_ == TimedWaitResult::NONE && !timed_wait_pin_active_ &&
               timed_wait_pin_generation_ == 0 &&
               kernel::timer::bsp_timer_engine().state(timed_wait_timer_) ==
                   kernel::timer::PrecisionTimerState::IDLE &&
               !timed_wait_worklet_.pending();
    }

    Thread::~Thread() noexcept {
        const auto &entity     = scheduler_storage_.entity;
        const auto timer_state = kernel::timer::bsp_timer_engine().state(timed_wait_timer_);
        if (entity.queue_hook.in_list || entity.queue_state != scheduler::QueueState::DETACHED ||
            entity.run_queue != nullptr || scheduler_attached_ || block_token_active_ ||
            state_ == ThreadState::RUNNING || state_ == ThreadState::BLOCKING ||
            timed_wait_state_ != TimedWaitState::IDLE ||
            timed_wait_result_ != TimedWaitResult::NONE || timed_wait_pin_active_ ||
            timed_wait_pin_generation_ != 0 ||
            timer_state != kernel::timer::PrecisionTimerState::IDLE ||
            timed_wait_worklet_.pending())
            tay::panic("destroying a Thread that is scheduled or has an active timed wait");
        process_->detach_thread(*this);
    }
}  // namespace task
