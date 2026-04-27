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
    class RR {
    public:
        using SUType                          = SU;
        constexpr static ClassType CLASS_TYPE = ClassType::RR;
        SchedMeta *cursched                   = nullptr;
        constexpr static int TIME_SLICES = 5;

    private:
        inline util::nonnull<SchedMeta *> asmeta(util::nonnull<SUType *> unit) {
            return SchedMeta::as_entity(unit);
        }

        inline util::nonnull<SUType *> asunit(util::nonnull<SchedMeta *> meta) {
            return SchedMeta::as_su<SUType>(meta);
        }

        inline util::nonnull<Entity *> as_entity(util::nonnull<SUType *> unit) {
            return util::guarantee_nonnull(&unit->rr_entity);
        }

        inline util::nonnull<Entity *> as_entity(
            util::nonnull<SchedMeta *> meta) {
            return as_entity(asunit(meta));
        }

    public:
        Result<util::nonnull<SUType *>> current() {
            if (cursched == nullptr) {
                unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
            }
            return asunit(util::guarantee_nonnull(cursched));
        }

        Result<void> enqueue(util::nonnull<RQ *> rq,
                             util::nonnull<SUType *> unit) {
            auto meta   = asmeta(unit);
            meta->state = ThreadState::READY;
            rq->rr_list.push_back(*meta);
            void_return();
        }

        Result<void> dequeue(util::nonnull<RQ *> rq,
                             util::nonnull<SUType *> unit) {
            auto meta = asmeta(unit);
            if (!rq->rr_list.contains(*meta)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            rq->rr_list.remove(*meta);
            meta->state = ThreadState::EMPTY;
            void_return();
        }

        Result<util::nonnull<SUType *>> pick_next(util::nonnull<RQ *> rq) {
            if (rq->rr_list.empty())
                unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
            // fetch the first task in the queue
            SchedMeta &meta = rq->rr_list.front();
            meta.state      = ThreadState::RUNNING;
            auto entity    = as_entity(util::nonnull_from(meta));
            entity->slice_cnt = TIME_SLICES;
            return asunit(util::nonnull_from(meta));
        }

        Result<void> put_prev(util::nonnull<RQ *> rq,
                              util::nonnull<SUType *> unit) {
            auto meta   = asmeta(unit);
            meta->state = ThreadState::READY;
            rq->rr_list.push_back(*meta);
            void_return();
        }

        Result<void> yield(util::nonnull<RQ *> rq) {
            // 为当前进程添加 NEED_RESCHED 标志
            if (cursched != nullptr) {
                cursched->flags_set<SchedMeta::FLAGS_NEED_RESCHED>();
            }
            void_return();
        }

        Result<void> on_tick(util::nonnull<RQ *> rq,
                             util::nonnull<SUType *> unit) {
            auto entity = as_entity(unit);
            if (entity->slice_cnt > 0) {
                entity->slice_cnt--;
            }
            if (entity->slice_cnt == 0) {
                // 时间片用尽，添加 NEED_RESCHED 标志
                util::nonnull<SchedMeta *> meta = asmeta(unit);
                meta->flags_set<SchedMeta::FLAGS_NEED_RESCHED>();
            }
            void_return();
        }
    };
}  // namespace schd::rr