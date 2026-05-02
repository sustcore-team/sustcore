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
            return _rq;
        }

        constexpr util::nonnull<rr::RR<TCB> *> rr_schd() {
            return _rr_schd;
        }

        constexpr util::nonnull<fcfs::FCFS<TCB> *> fcfs_schd() {
            return _fcfs_schd;
        }

        constexpr util::nonnull<idle::IDLE<TCB> *> idle_schd() {
            return _idle_schd;
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

        /**
         * @brief 按优先级遍历调度器类
         *
         * 该函数会按照优先级顺序(从高到低)遍历调度器类, 并对每个调度器类调用传入的函数对象
         * 当对应调度器类优先级低于指定的bot时, 遍历将会停止
         * 
         * @tparam Func 函数对象类型, 期望类型为: BaseSchedPtr -> void
         * @param f 函数对象, 将会被调用来处理每个调度器类
         * @param bot 遍历的优先级下界, 默认为 ClassType::BOT, 即遍历所有调度器类
         */
        template <typename Func>
        void foreach_schdclass(Func f, ClassType bot = ClassType::BOT) {
            if (ClassType::RR < bot) {
                f(rr_schd());
            }
            if (ClassType::FCFS < bot) {
                f(fcfs_schd());
            }
            if (ClassType::IDLE < bot) {
                f(idle_schd());
            }
        }

        Result<util::nonnull<TCB *>> pick_next_task();
        void check_preempt_curr(TCB *new_tcb);

        void switch_to(TCB *tcb);

        bool try_wakeup(TCB *tcb, int flags);
        bool wakeup(TCB *tcb);
        bool wakeup_new(TCB *new_tcb);

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