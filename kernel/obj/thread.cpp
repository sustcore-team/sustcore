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
#include <timer/hrtimer.h>

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
          sched_snapshot_(ThreadState::RUNNING),
          configured_(true),
          sched_attached_(true) {
        scheduler::ThreadSchedOps::initialize(*this);
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
        scheduler::ThreadSchedOps::initialize(*this);
        context_.ra() = reinterpret_cast<addr_t>(&scheduler::scheduler_thread_bootstrap);
        context_.sp() = stack_.top();
        (void)process.attach_thread(*this);
    }

    tay::expected<cap::KObjectRef<Thread>, ThreadError> Thread::create_kernel(
        Process &process, ThreadEntry entry, void *argument) noexcept {
        if (!process.kernel())
            return tay::Err(ThreadError::InvalidMode(ThreadMode::KERNEL));
        return create_kernel_for(process, entry, argument);
    }

    tay::expected<cap::KObjectRef<Thread>, ThreadError> Thread::create_kernel_for(
        Process &process, ThreadEntry entry, void *argument) noexcept {
        if (process.state() != ProcessState::SUBMITTED)
            return tay::Err(ThreadError::InvalidProcessState(process.state()));
        if (entry == nullptr)
            return tay::Err(ThreadError::InvalidEntry(0));
        auto stack   = TAY_TRY(KernelStack::create());
        auto *thread = new (std::nothrow)
            Thread(process, std::move(stack), ThreadMode::KERNEL, entry, argument);
        if (thread == nullptr)
            return tay::Err(ThreadError::OutOfMemory());
        return cap::KObjectRef<Thread>(*thread);
    }

    tay::expected<cap::KObjectRef<Thread>, ThreadError> Thread::create_user(
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
        return cap::KObjectRef<Thread>(*thread);
    }

    tay::expected<void, ThreadError> Thread::configure_user(addr_t entry, addr_t stack_pointer,
                                                            addr_t argument) noexcept {
        if (mode_ != ThreadMode::USER)
            return tay::Err(ThreadError::InvalidMode(mode_));
        if (sched_attached_)
            return tay::Err(ThreadError::SchedulerAttached());
        if (state_ != ThreadState::CREATED || configured_)
            return tay::Err(ThreadError::AlreadyConfigured());
        if (entry == 0)
            return tay::Err(ThreadError::InvalidEntry(entry));
        if (stack_pointer == 0 || (stack_pointer & 0xf) != 0)
            return tay::Err(ThreadError::InvalidUserStack(stack_pointer));
        hal::init_user_frame(user_frame_, entry, stack_pointer, argument);
        configured_ = true;
        return {};
    }

    void Thread::WaitWorklet::setup(Thread &thread, u64_t generation) noexcept {
        if (generation == 0)
            kernel::log::panic("configuring a timed-wait Worklet with an invalid generation");
        if (pending())
            kernel::log::panic("configuring a pending timed-wait Worklet");
        thread_     = &thread;
        generation_ = generation;
        owner_ref_  = cap::KObjectRef<Thread>(thread);
    }

    void Thread::WaitWorklet::run() noexcept {
        // setup() 在 timer arm 前已建立强引用；先移动它，再读取嵌入的 Thread 状态。
        auto owner_ref        = std::move(owner_ref_);
        auto *thread          = thread_;
        const auto generation = generation_;
        thread_               = nullptr;
        generation_           = 0;
        if (thread == nullptr || generation == 0)
            kernel::log::panic("timed-wait Worklet has no typed completion target");
        if (!owner_ref)
            kernel::log::panic("timed-wait Worklet lost its completion owner reference");

        bool wake_waiter = false;
        {
            auto state = thread->wait_.lock();
            if (generation == state->generation && state->state == TimedWaitState::PENDING) {
                state->state  = TimedWaitState::COMPLETED;
                state->result = TimedWaitResult::TIMEOUT;
                // Completion may race the waiter's two-phase park.  A wake is
                // valid only after the waiter has published that it is parked;
                // wait_until() handles the earlier completion after prepare.
                wake_waiter   = state->parked;
            }
        }

        auto &engine = kernel::timer::bsp_hrtimers();
        engine.retire(thread->wait_timer_);
        engine.reset(thread->wait_timer_);

        // 本地中断保持关闭直到 unpin，避免 BSP waiter 在 bookkeeping 清零与实际释放 pin
        // 之间重新运行；Worklet 的 owner_ref_ 同时覆盖跨 CPU waiter 的生命周期。
        hal::irq_guard irq_guard;
        bool release_pin = false;
        {
            auto state = thread->wait_.lock();
            if (state->pin_active && state->pin_generation == generation) {
                state->pin_active     = false;
                state->pin_generation = 0;
                release_pin           = true;
            }
        }
        if (!release_pin)
            kernel::log::panic("timed-wait Worklet did not own exactly one Thread pin");

        // wake 可能让 waiter 退出并释放 scheduler 强引用；pin 在此之前覆盖整个 cleanup。
        if (wake_waiter)
            scheduler::wake(*thread, scheduler::WakeReason::WAIT_COMPLETED);

        // 最后一个 pin 释放后仍由 owner_ref 保持 Thread 存活；引用离开作用域后才可
        // 销毁 Thread（并连带销毁本 Worklet），此后不得访问任何成员。
        thread->unpin();
    }

    tay::expected<TimedWaitResult, ThreadError> Thread::wait_until(units::time deadline) noexcept {
        auto &core = scheduler::local();
        if (core.current() != this)
            return tay::Err(ThreadError::NotCurrentThread());
        if (state_ != ThreadState::RUNNING)
            return tay::Err(ThreadError::InvalidThreadState(state_));

        hal::irq_guard irq_guard;
        u64_t generation = 0;
        {
            auto state = wait_.lock();
            // 配置检查固定按 timed-wait -> timer engine 取锁；engine 路径不会反向取得本锁。
            if (state->state != TimedWaitState::IDLE || state->result != TimedWaitResult::NONE ||
                state->pin_active || state->pin_generation != 0 ||
                kernel::timer::bsp_hrtimers().state(wait_timer_) !=
                    kernel::timer::HRTState::IDLE ||
                wait_work_.pending())
                return tay::Err(ThreadError::TimedWaitActive());
            ++state->generation;
            if (state->generation == 0)
                ++state->generation;
            generation = state->generation;
            if (!try_pin())
                return tay::Err(ThreadError::PinFailed());
            state->pin_active     = true;
            state->pin_generation = generation;
            state->state          = TimedWaitState::PENDING;
            state->result         = TimedWaitResult::NONE;
            state->parked         = false;
            wait_work_.setup(*this, generation);
        }

        auto armed = kernel::timer::bsp_hrtimers().arm(wait_timer_, deadline, wait_work_);
        if (!armed) {
            {
                auto state            = wait_.lock();
                state->state          = TimedWaitState::IDLE;
                state->pin_active     = false;
                state->pin_generation = 0;
                state->parked         = false;
            }
            wait_work_.clear_owner();
            unpin();
            return tay::Err(ThreadError::TimerArmFailed(armed.error().code()));
        }

        auto token = core.prepare_block();
        if (!token)
            kernel::log::panic("timed wait failed to prepare its two-phase park");
        bool completed_before_commit = false;
        {
            auto state              = wait_.lock();
            state->parked           = true;
            completed_before_commit = state->state == TimedWaitState::COMPLETED;
        }
        if (completed_before_commit)
            scheduler::wake(*this, scheduler::WakeReason::WAIT_COMPLETED);
        if (auto committed = core.commit_block(std::move(*token)); !committed)
            kernel::log::panic("timed wait failed to commit its two-phase park");

        // 显式 winner 可以先唤醒 Thread；在 completion retire/reset 并清 pin 前禁止复用节点。
        while (true) {
            {
                auto state = wait_.lock();
                if (state->state == TimedWaitState::COMPLETED && !state->pin_active)
                    break;
            }
            scheduler::yield();
        }

        TimedWaitResult result;
        {
            auto state    = wait_.lock();
            result        = state->result;
            state->state  = TimedWaitState::IDLE;
            state->result = TimedWaitResult::NONE;
            state->parked = false;
        }
        return result;
    }

    bool Thread::finish_wait(u64_t generation, TimedWaitResult result) noexcept {
        bool wake_waiter = false;
        {
            auto state = wait_.lock();
            if (generation == 0 || generation != state->generation ||
                state->state != TimedWaitState::PENDING)
                return false;
            state->state  = TimedWaitState::COMPLETED;
            state->result = result;
            wake_waiter   = state->parked;
        }

        // completion 可能已经摘下 Worklet 并开始 retire；NOT_ARMED 表示 cleanup 已越过 cancel 点。
        (void)kernel::timer::bsp_hrtimers().cancel(wait_timer_);
        if (wake_waiter)
            scheduler::wake(*this, scheduler::WakeReason::WAIT_COMPLETED);
        return true;
    }

    bool Thread::wake_wait(u64_t generation) noexcept {
        return finish_wait(generation, TimedWaitResult::WOKEN);
    }

    bool Thread::cancel_wait(u64_t generation) noexcept {
        return finish_wait(generation, TimedWaitResult::CANCELLED);
    }

    u64_t Thread::wait_gen() noexcept {
        auto state = wait_.lock();
        return state->state == TimedWaitState::PENDING ? state->generation : 0;
    }

    bool Thread::wait_idle() noexcept {
        auto state = wait_.lock();
        // scheduler detach 可持 run queue lock 调用本函数，固定锁序为 scheduler -> timed-wait
        // -> timer engine；timer completion 分段释放各锁后才调用 scheduler。
        return state->state == TimedWaitState::IDLE && state->result == TimedWaitResult::NONE &&
               !state->pin_active && state->pin_generation == 0 &&
               kernel::timer::bsp_hrtimers().state(wait_timer_) ==
                   kernel::timer::HRTState::IDLE &&
               !wait_work_.pending();
    }

    Thread::~Thread() noexcept {
        const auto &entity     = sched_.entity;
        auto timed_wait        = wait_.lock();
        const auto timer_state = kernel::timer::bsp_hrtimers().state(wait_timer_);
        if (entity.queue_hook.in_list || entity.queue_state != scheduler::QueueState::DETACHED ||
            entity.run_queue != nullptr || sched_attached_ || block_token_active_ ||
            state_ == ThreadState::RUNNING || state_ == ThreadState::BLOCKING ||
            timed_wait->state != TimedWaitState::IDLE ||
            timed_wait->result != TimedWaitResult::NONE || timed_wait->pin_active ||
            timed_wait->pin_generation != 0 || timer_state != kernel::timer::HRTState::IDLE ||
            wait_work_.pending())
            tay::panic("destroying a Thread that is scheduled or has an active timed wait");
        process_->detach_thread(*this);
    }
}  // namespace task
