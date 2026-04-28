/**
 * @file scheduler.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief scheduler
 * @version alpha-1.0.0
 * @date 2026-03-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <schd/idle.h>
#include <schd/schdbase.h>
#include <sus/nonnull.h>
#include <sustcore/errcode.h>
#include <task/task_struct.h>

namespace schd {
    class Scheduler {
    private:
        RQ _rq;

        rr::RR<TCB> _rr_schd;
        fcfs::FCFS<TCB> _fcfs_schd;
        idle::IDLE<TCB> _idle_schd;

    public:
        constexpr Scheduler(util::nonnull<TCB *> init_tcb) : _idle_schd(init_tcb) {}

        constexpr util::nonnull<RQ *> rq() {
            return util::nonnull_from(_rq);
        }

        constexpr util::nonnull<rr::RR<TCB> *> rr_schd() {
            return util::nonnull_from(_rr_schd);
        }

        constexpr util::nonnull<fcfs::FCFS<TCB> *> fcfs_schd() {
            return util::nonnull_from(_fcfs_schd);
        }

        constexpr util::nonnull<idle::IDLE<TCB> *> idle_schd() {
            return util::nonnull_from(_idle_schd);
        }

        using BaseSchedPtr = util::nonnull<BaseSched<TCB> *>;

        constexpr Result<BaseSchedPtr> schd(ClassType type) {
            switch (type) {
                case ClassType::RR:   return {rr_schd()};
                case ClassType::FCFS: return {fcfs_schd()};
                case ClassType::IDLE: return {idle_schd()};
                default:              unexpect_return(ErrCode::INVALID_PARAM);
            }
        }

    private:
        TCB *_curtcb;
        PCB *_curpcb;

        // foreach the schdclass in priority order and
        // apply the function f to it
        // RR > FCFS
        template <typename Func>
        void foreach_schdclass(Func f) {
            f(rr_schd());
            f(fcfs_schd());
            f(idle_schd());
        }

        Result<util::nonnull<TCB *>> pick_next_task();

        void switch_to(TCB *tcb);

    public:
        void do_tick(const TimerTickEvent &e);

        void init();

        /**
         * @brief 调度入口
         * 进入后将会判断是否需要进行调度,
         * 如果需要则选择下一个要运行的调度单元并切换到它 注意的是,
         * 只有在current_tcb不为null时, 调度器才开始工作 这意味着, 你需要通过
         * init() 方法将 current_tcb 设置为一个有效的 TCB 后, 调度器才会开始调度
         * 一般来说, init() 方法会将其设置为 IDLE 调度类中的 IDLE TCB,
         * 这样调度器就会在没有其他可运行线程时调度到这个 IDLE 线程上
         *
         */
        void schedule();

        // 任务入队/唤醒
        Result<void> enqueue(util::nonnull<TCB *> tcb);

        // 任务出队/阻塞
        Result<void> dequeue(util::nonnull<TCB *> tcb);

        // 主动放弃 CPU
        void yield();
    };
}  // namespace schd