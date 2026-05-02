/**
 * @file rr.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Round Robin 调度器
 * @version alpha-1.0.0
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <schd/schdbase.h>
#include <sus/list.h>
#include <sus/nonnull.h>
#include <sus/units.h>
#include <sustcore/errcode.h>

namespace schd::rr {
    struct Entity {
        int slice_cnt = 0;
    };

    template <typename SU>
    class RR : public BaseSched<SU> {
    public:
        using SUType                          = SU;
        constexpr static ClassType CLASS_TYPE = ClassType::RR;
        constexpr static int TIME_SLICES = 5;

    private:
        inline util::nonnull<Entity *> as_entity_rr(util::nonnull<SUType *> unit) {
            return util::nnullforce(&unit->rr_entity);
        }

        inline util::nonnull<Entity *> as_entity_rr(
            util::nonnull<SchedMeta *> meta) {
            return as_entity_rr(this->asunit(meta));
        }

    public:
        Result<void> enqueue(util::nonnull<RQ *> rq,
                             util::nonnull<SUType *> unit) override {
            auto meta   = this->asmeta(unit);
            meta->state = ThreadState::READY;
            rq->rr_list.push_back(*meta);
            void_return();
        }

        Result<void> dequeue(util::nonnull<RQ *> rq,
                             util::nonnull<SUType *> unit) override {
            auto meta = this->asmeta(unit);
            if (!rq->rr_list.contains(*meta)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            rq->rr_list.remove(*meta);
            meta->state = ThreadState::EMPTY;
            void_return();
        }

        Result<util::nonnull<SUType *>> pick_next(util::nonnull<RQ *> rq) override {
            if (rq->rr_list.empty())
                unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
            // fetch the first task in the queue
            SchedMeta &meta = rq->rr_list.front();
            meta.state      = ThreadState::RUNNING;
            auto entity    = as_entity_rr(meta);
            entity->slice_cnt = TIME_SLICES;
            this->cursched = &meta;
            return this->asunit(meta);
        }

        Result<void> put_prev(util::nonnull<RQ *> rq,
                              util::nonnull<SUType *> unit) override {
            auto meta   = this->asmeta(unit);
            meta->state = ThreadState::READY;
            rq->rr_list.push_back(*meta);
            void_return();
        }

        Result<void> yield(util::nonnull<RQ *> rq) override {
            // 为当前进程添加 NEED_RESCHED 标志
            if (this->cursched != nullptr) {
                this->cursched->template flags_set<SchedMeta::FLAGS_NEED_RESCHED>();
            }
            void_return();
        }

        /**
         * @brief 每个tick调用一次, 用于更新调度单元的状态
         *
         * 对 RR 调度器, 该函数的主要作用是检查当前正在运行的调度单元的时间片是否用尽,
         * 如果用尽了就为它添加 NEED_RESCHED 标志
         * 
         * @param rq 
         * @param unit 
         * @return Result<void> 
         */
        Result<void> on_tick(util::nonnull<RQ *> rq,
                             util::nonnull<SUType *> unit) override {
            auto entity = as_entity_rr(unit);
            if (entity->slice_cnt > 0) {
                entity->slice_cnt--;
            }
            if (entity->slice_cnt == 0) {
                // 时间片用尽，添加 NEED_RESCHED 标志
                util::nonnull<SchedMeta *> meta = this->asmeta(unit);
                meta->flags_set<SchedMeta::FLAGS_NEED_RESCHED>();
            }
            void_return();
        }

        bool check_preempt_curr(util::nonnull<RQ *> rq, util::nonnull<SUType *> new_su) override {
            // 只要对方的级别比自己高, 就需要抢占当前任务
            return true;
        }
    };
}  // namespace schd::rr