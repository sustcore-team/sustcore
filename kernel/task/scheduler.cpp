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

#include <arch/riscv64/description.h>
#include <env.h>
#include <mem/vma.h>
#include <sus/nonnull.h>
#include <task/scheduler.h>

namespace key {
    using namespace env::key;
    struct schd : public tmm {
    public:
        schd() = default;
    };
}  // namespace key

namespace schd {
    void switch_kstack(void *kstack_top) {
        // 切换到新的内核栈
        // 我们通过切换 sscratch 寄存器来切换内核栈
        Context::switch_to(kstack_top);
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

    void Scheduler::switch_to(TCB *tcb) {
        // 切换页表
        switch_pgd(tcb->task->tmm);
        // 切换内核栈
        switch_kstack(tcb->kstack_top);
        _curtcb = tcb;
        _curpcb = tcb->task;
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
        return util::guarantee_nonnull(next);
    }

    void Scheduler::schedule() {
        // 首先获得当前正在运行的线程
        if (_curtcb == nullptr) {
            // 没有正在运行的线程, 调度器未启动, 直接返回
            return;
        }

        // TODO: 允许高位调度类抢占低位调度类的线程

        // 判断是否需要重新调度
        if (!_curtcb->basic_entity
                 .flags_check<SchedMeta::FLAGS_NEED_RESCHED>())
        {
            return;
        }

        // 如果需要重新调度, 则将当前线程放回就绪队列
        auto schd_res = schd(_curtcb->schd_class);
        if (!schd_res.has_value()) {
            loggers::SUSTCORE::ERROR("未知的调度类! 错误码: %s",
                                     to_cstring(schd_res.error()));
            panic("调度器崩溃!");
        }
        auto schdclass = schd_res.value();
        auto put_prev_res =
            schdclass->put_prev(rq(), util::guarantee_nonnull(_curtcb));
        if (!put_prev_res.has_value()) {
            loggers::SUSTCORE::ERROR(
                "调度器处理put_prev失败! 错误码: %s 对应调度类: %s",
                to_cstring(put_prev_res.error()),
                to_cstring(_curtcb->schd_class));
            panic("调度器崩溃!");
        }

        // 选择下一个要运行的线程
        auto next_res = pick_next_task();
        if (!next_res.has_value()) {
            loggers::SUSTCORE::ERROR("没有可运行的线程! 错误码: %s",
                                     to_cstring(next_res.error()));
            panic("调度器崩溃!");
        }
        TCB *next = next_res.value();
        switch_to(next);
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

    void Scheduler::yield() {
        if (_curtcb == nullptr) {
            return;
        }

        auto tcb_ptr  = util::guarantee_nonnull(_curtcb);
        auto schd_res = schd(tcb_ptr->schd_class);
        if (schd_res.has_value()) {
            auto yield_res = schd_res.value()->yield(rq());
            if (!yield_res.has_value()) {
                loggers::SUSTCORE::ERROR("调度器处理yield失败! 错误码: %s",
                                         to_cstring(yield_res.error()));
            }
        }

        schedule();
    }

    // RR > FCFS
    void Scheduler::do_tick(const TimerTickEvent &e) {
        if (_curtcb == nullptr) {
            return;
        }

        auto tcb      = util::guarantee_nonnull(_curtcb);
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
        // 将当前线程设置为 IDLE 调度类中的 IDLE 线程
        auto idle_res = idle_schd()->pick_next(rq());
        if (!idle_res.has_value()) {
            loggers::SUSTCORE::ERROR("IDLE调度器没有可运行的线程! 错误码: %s",
                                     to_cstring(idle_res.error()));
            panic("调度器崩溃!");
        }
        auto yield_res = idle_schd()->yield(rq());
        if (!yield_res.has_value()) {
            loggers::SUSTCORE::ERROR("IDLE调度器处理yield失败! 错误码: %s",
                                     to_cstring(yield_res.error()));
            panic("调度器崩溃!");
        }
        _curtcb = idle_res.value();
        _curpcb = _curtcb->task;
    }
}  // namespace schd