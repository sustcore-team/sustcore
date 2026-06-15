/**
 * @file scheduler.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief scheduler
 * @version alpha-1.0.0
 * @date 2026-03-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/description.h>
#include <device/int.h>
#include <env.h>
#include <logger.h>
#include <mem/vma.h>
#include <sus/nonnull.h>
#include <syscall/syscall.h>
#include <task/scheduler.h>
#include <task/wait.h>

#include <cassert>
#include <new>

namespace {
    alignas(schd::Scheduler) unsigned char scheduler_storage[sizeof(
        schd::Scheduler)];
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    schd::Scheduler *inst_scheduler = nullptr;
    bool inst_scheduler_initialized = false;
}  // namespace

namespace key {
    using namespace env::key;
    struct schd : public tmm {
    public:
        schd() = default;
    };
}  // namespace key

extern "C"
void __switch_to(Context *prev_ctx, Context *next_ctx);

namespace schd {
    using namespace task;
    void Scheduler::init(util::nonnull<TCB *> idle_tcb,
                         util::nonnull<TCB *> kinit_tcb) {
        inst_scheduler = new (scheduler_storage) Scheduler(idle_tcb, kinit_tcb);
        inst_scheduler_initialized = true;
    }

    bool Scheduler::initialized() {
        return inst_scheduler_initialized && inst_scheduler != nullptr;
    }

    Scheduler &Scheduler::inst() {
        if (!initialized()) {
            panic("Scheduler 未初始化!");
        }
        return *inst_scheduler;
    }

    util::nonnull<RQ *> Scheduler::rq() {
        return util::nnullforce(env::hart_ctx->rq());
    }

    TCB *Scheduler::current_tcb() const {
        return env::hart_ctx->current_tcb();
    }

    PCB *Scheduler::current_pcb() const {
        return env::hart_ctx->current_pcb();
    }

    Result<void> Scheduler::preempt_disable() noexcept {
        auto *current = current_tcb();
        if (current == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        current->basic_entity
            .template flags_set<SchedMeta::FLAGS_PREEMPT_DISABLED>();
        void_return();
    }

    Result<void> Scheduler::preempt_enable() noexcept {
        auto *current = current_tcb();
        if (current == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        current->basic_entity
            .template flags_reset<SchedMeta::FLAGS_PREEMPT_DISABLED>();
        void_return();
    }

    bool Scheduler::preempt_disabled() const noexcept {
        auto *current = current_tcb();
        if (current == nullptr) {
            return false;
        }
        return current->basic_entity
            .template flags_check<SchedMeta::FLAGS_PREEMPT_DISABLED>();
    }

    void switch_pgd(TaskMemoryManager *tmm) {
        // 只在页表不为null且不等于当前页表时才切换
        if (tmm->pgd().nonnull() && tmm->pgd() != env::inst().pgd()) {
            PageMan(tmm->pgd()).switch_root();
            PageMan(tmm->pgd()).flush_tlb();
        }
        // 更新 environment 中的 task memory
        env::inst().tmm(key::schd()) = tmm;
    }

    void Scheduler::prepare_switch(TCB *tcb) {
        // 切换页表
        switch_pgd(tcb->task->tmm);
        env::hart_ctx->current_tcb() = tcb;
        env::hart_ctx->current_pcb() = tcb->task;
    }

    void Scheduler::switch_to(TCB *prev, TCB *next) {
        assert(prev != nullptr);
        assert(next != nullptr);
        if (!Interrupt::enabled()) {
            loggers::SUSTCORE::FATAL("禁止在关中断临界区内调用 switch_to");
        }
        __switch_to(prev->kernel_context_ptr(), next->kernel_context_ptr());
    }

    bool Scheduler::try_wakeup(TCB *tcb, int flags) {
        // TODO: 实现flags并判断tcb是否满足唤醒条件
        // 我先不做, 等着后面实现睡眠和唤醒机制的时候再说
        // 目前的唯一用处就是执行 check_preempt_curr 并 enqueue 线程
        auto enqueue_res = enqueue(util::nnullforce(tcb));
        if (!enqueue_res.has_value()) {
            loggers::SUSTCORE::ERROR("调度器处理enqueue失败! 错误码: %s",
                                     to_cstring(enqueue_res.error()));
            return false;
        }
        check_preempt_curr(tcb);
        return true;
    }

    bool Scheduler::wakeup(TCB *tcb) {
        return try_wakeup(tcb, 0);
    }

    bool Scheduler::wakeup_new(TCB *new_tcb) {
        // 我们目前直接执行try_wakeup
        return try_wakeup(new_tcb, 0);
    }

    Result<util::nonnull<TCB *>> Scheduler::pick_next_task() {
        TCB *next = nullptr;
        foreach_schdclass([&](auto &&schd_inst) {
            if (next) {
                return;
            }
            auto res = schd_inst->pick_next(rq());
            if (res.has_value()) {
                next = res.value();
            }
        });

        if (next == nullptr) {
            unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
        }
        return util::nnullforce(next);
    }

    Result<void> Scheduler::prepare_prev_task(TCB *tcb) noexcept {
        if (tcb == nullptr || tcb->basic_entity.state == ThreadState::WAITING) {
            void_return();
        }
        if (!can_schedule_tcb(tcb)) {
            tcb->basic_entity.state = ThreadState::WAITING;
            void_return();
        }

        auto schd_res = schd(tcb->schd_class);
        if (!schd_res.has_value()) {
            unexpect_return(schd_res.error());
        }
        return schd_res.value()->put_prev(rq(), util::nnullforce(tcb));
    }

    bool Scheduler::can_schedule_tcb(TCB *tcb) noexcept {
        if (tcb == nullptr) {
            return false;
        }
        return true;
    }

    Result<util::nonnull<TCB *>> Scheduler::prepare_next_task() {
        while (true) {
            auto next_res = pick_next_task();
            propagate(next_res);
            TCB *next = next_res.value();
            prepare_switch(next);

            if (can_schedule_tcb(next)) {
                return util::nnullforce(next);
            }

            next->basic_entity
                .template flags_set<SchedMeta::FLAGS_NEED_RESCHED>();
        }
    }

    void Scheduler::check_preempt_curr(TCB *new_tcb) {
        auto *current = current_tcb();
        if (current == nullptr) {
            // 没有正在运行的线程, 不需要抢占
            return;
        }
        if (current->basic_entity
                .template flags_check<SchedMeta::FLAGS_PREEMPT_DISABLED>())
        {
            return;
        }

        bool do_preempt = false;
        if (new_tcb->schd_class <= current->schd_class) {
            // 如果新线程的调度类优先级不高于当前线程, 则不需要抢占
            return;
        }

        // 否则, 需要询问新线程的调度类是否需要抢占当前线程
        auto schd_res = schd(new_tcb->schd_class);
        if (!schd_res.has_value()) {
            loggers::SUSTCORE::ERROR("未知的调度类! 错误码: %s",
                                     to_cstring(schd_res.error()));
            return;
        }

        do_preempt = schd_res.value()->check_preempt_curr(
            rq(), util::nnullforce(new_tcb));

        if (do_preempt) {
            // 为当前线程添加 NEED_RESCHED 标志,
            // 让调度器在合适的时候切换到新线程
            current->basic_entity
                .template flags_set<SchedMeta::FLAGS_NEED_RESCHED>();
        }
    }

    void Scheduler::schedule(bool ignore_preempt_disabled) {
        // 首先获得当前正在运行的线程
        auto *current = current_tcb();
        if (current == nullptr) {
            // 没有正在运行的线程, 调度器未启动, 直接返回
            return;
        }

        if (!ignore_preempt_disabled && current->basic_entity.state !=
                                            ThreadState::WAITING &&
            current->basic_entity
                .template flags_check<SchedMeta::FLAGS_PREEMPT_DISABLED>())
        {
            return;
        }

        // 判断是否需要重新调度
        if (!current->basic_entity.flags_check<SchedMeta::FLAGS_NEED_RESCHED>())
        {
            return;
        }
        current->basic_entity
            .template flags_reset<SchedMeta::FLAGS_NEED_RESCHED>();

        auto put_prev_res = prepare_prev_task(current);
        if (!put_prev_res.has_value()) {
            loggers::SUSTCORE::ERROR(
                "调度器处理put_prev失败! 错误码: %s 对应调度类: %s",
                to_cstring(put_prev_res.error()),
                to_cstring(current->schd_class));
            panic("调度器崩溃!");
        }

        // 选择下一个要运行的线程
        TCB *prev = current;
        auto next_res = prepare_next_task();
        if (!next_res.has_value()) {
            loggers::SUSTCORE::ERROR("没有可运行的线程! 错误码: %s",
                                     to_cstring(next_res.error()));
            panic("调度器崩溃!");
        }
        TCB *next = next_res.value();
        if (prev != next && prev != nullptr) {
            switch_to(prev, next);
        }
    }

    Result<void> Scheduler::enqueue(util::nonnull<TCB *> tcb) {
        auto schd_res = schd(tcb->schd_class);
        if (!schd_res.has_value()) {
            unexpect_return(schd_res.error());
        }

        return schd_res.value()->enqueue(rq(), tcb);
    }

    Result<void> Scheduler::dequeue(util::nonnull<TCB *> tcb) {
        auto schd_res = schd(tcb->schd_class);
        if (!schd_res.has_value()) {
            unexpect_return(schd_res.error());
        }

        return schd_res.value()->dequeue(rq(), tcb);
    }

    Result<void> Scheduler::block_current(wait::wd_t wait_wd) {
        return block_current(wait_wd, {});
    }

    Result<void> Scheduler::block_current(wait::wd_t wait_wd,
                                          wait::WaitPredicate predicate) {
        auto *current = current_tcb();
        if (current == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (current->schd_class == ClassType::IDLE) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto enqueue_res = wait::WaitReasonManager::inst().enqueue(
            wait_wd, current, std::move(predicate));
        propagate(enqueue_res);

        current->basic_entity
            .template flags_set<SchedMeta::FLAGS_NEED_RESCHED>();
        schedule(true);
        void_return();
    }

    bool Scheduler::wakeup_waiting(TCB *tcb) {
        if (tcb == nullptr || tcb->basic_entity.state != ThreadState::WAITING) {
            return false;
        }
        tcb->basic_entity.state = ThreadState::EMPTY;
        return wakeup(tcb);
    }

    void Scheduler::yield() {
        auto *current = current_tcb();
        if (current == nullptr) {
            return;
        }

        auto tcb_ptr  = util::nnullforce(current);
        auto schd_res = schd(tcb_ptr->schd_class);
        if (schd_res.has_value()) {
            auto yield_res = schd_res.value()->yield(rq());
            if (!yield_res.has_value()) {
                loggers::SUSTCORE::ERROR("调度器处理yield失败! 错误码: %s",
                                         to_cstring(yield_res.error()));
            }
        }

        schedule(true);
    }

    // RR > FCFS
    void Scheduler::do_tick(const TimerTickEvent &e) {
        loggers::TASK::DEBUG(
            "调度 tick: last=%llu now=%llu delta=%llu",
            static_cast<unsigned long long>(e.last.to_nanoseconds()),
            static_cast<unsigned long long>(e.now.to_nanoseconds()),
            static_cast<unsigned long long>(e.delta.to_nanoseconds()));
        auto *current = current_tcb();
        if (current == nullptr) {
            return;
        }

        auto tcb      = util::nnullforce(current);
        auto schd_res = schd(tcb->schd_class);
        if (!schd_res.has_value()) {
            loggers::SUSTCORE::ERROR("未知的调度类! 错误码: %s",
                                     to_cstring(schd_res.error()));
            return;
        }

        auto tick_res = schd_res.value()->on_tick(rq(), tcb);
        if (!tick_res.has_value()) {
            loggers::SUSTCORE::ERROR(
                "调度器处理on_tick失败! 错误码: %s 对应调度类: %s",
                to_cstring(tick_res.error()), to_cstring(tcb->schd_class));
        }
    }

    void Scheduler::init() {
        if (_idle_schd.ready == nullptr || _init_schd.kinit_ready == nullptr) {
            loggers::SUSTCORE::ERROR("调度器启动前实体无效");
            panic("调度器崩溃!");
        }
        if (current_tcb() != nullptr || current_pcb() != nullptr) {
            loggers::SUSTCORE::ERROR("调度器初始化阶段 current 不应已就绪");
            panic("调度器崩溃!");
        }
    }

    [[noreturn]]
    void Scheduler::bootstrap_tasks() {
        if (current_tcb() != nullptr || current_pcb() != nullptr) {
            loggers::SUSTCORE::ERROR("bootstrap_tasks 前 current 必须为空");
            panic("调度器崩溃!");
        }
        auto next_res = prepare_next_task();
        if (!next_res.has_value()) {
            loggers::SUSTCORE::ERROR("没有可运行线程, 无法启动调度: %s",
                                     to_cstring(next_res.error()));
            panic("调度器崩溃!");
        }
        auto next = next_res.value();
        prepare_switch(next);
        Context bootstrap_prev{};
        bootstrap_prev.sp() = 0;
        __switch_to(&bootstrap_prev, next->kernel_context_ptr());
        panic("bootstrap_tasks 不应返回");
    }
}  // namespace schd
