/**
 * @file fcfs.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief first come first serve scheduler
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
#include <sustcore/errcode.h>

namespace schd::fcfs {
    template <typename SU>
    class FCFS {
    public:
        using SUType                          = SU;
        constexpr static ClassType CLASS_TYPE = ClassType::FCFS;
        SchedMeta *cursched                   = nullptr;

    private:
        inline util::nonnull<SchedMeta *> asmeta(util::nonnull<SUType *> unit) {
            return SchedMeta::as_entity(unit);
        }

        inline util::nonnull<SUType *> asunit(util::nonnull<SchedMeta *> meta) {
            return SchedMeta::as_su<SUType>(meta);
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
            rq->fcfs_list.push_back(*meta);
            void_return();
        }

        Result<void> dequeue(util::nonnull<RQ *> rq,
                             util::nonnull<SUType *> unit) {
            auto meta = asmeta(unit);
            if (!rq->fcfs_list.contains(*meta)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            rq->fcfs_list.remove(*meta);
            meta->state = ThreadState::EMPTY;
            void_return();
        }

        Result<util::nonnull<SUType *>> pick_next(util::nonnull<RQ *> rq) {
            if (rq->fcfs_list.empty())
                unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
            // fetch the first task in the queue
            SchedMeta &meta = rq->fcfs_list.front();
            meta.state      = ThreadState::RUNNING;
            rq->fcfs_list.pop_front();
            return asunit(util::nonnull_from(meta));
        }

        Result<void> put_prev(util::nonnull<RQ *> rq,
                              util::nonnull<SUType *> unit) {
            auto meta   = asmeta(unit);
            meta->state = ThreadState::READY;
            rq->fcfs_list.push_back(&meta);
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
            // FCFS 不需要在时钟中断时进行任何操作
            void_return();
        }
    };
}  // namespace schd::fcfs