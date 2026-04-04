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

#include <schd/hook.h>
#include <schd/schdbase.h>
#include <sus/list.h>
#include <sustcore/errcode.h>

namespace schd::rr {
    class Entity {
    public:
        ThreadState state                = ThreadState::EMPTY;
        int slice_cnt                    = 0;
        util::ListHead<Entity> list_head = {};

        constexpr Entity()  = default;
        constexpr ~Entity() = default;
    };

    class tags : public schd::tags::on_tick {};

    template <typename SU>
    class RR : public SchdBase<SU, tags> {
    public:
        using SUType                     = SU;
        using EntityType                 = Entity;
        using Tags                       = tags;
        using Base                       = SchdBase<SUType, Tags>;
        constexpr static int TIME_SLICES = 5;
        static_assert(
            requires(SUType su) { su.rr_entity; },
            "SUType须具有类型为schd::rr::Entity的成员rr_entity");
        constexpr static size_t ENTITY_OFFSET = offsetof(SUType, rr_entity);

        constexpr static EntityType *get_entity(SUType *su) {
            return &su->rr_entity;
        }
        constexpr static const EntityType *get_entity(const SUType *su) {
            return &su->rr_entity;
        }
        constexpr static SUType *get_su(EntityType *entity) {
            return reinterpret_cast<SUType *>(reinterpret_cast<char *>(entity) -
                                              ENTITY_OFFSET);
        }

    private:
        util::IntrusiveList<EntityType> ready_queue;

    public:
        constexpr RR()  = default;
        constexpr ~RR() = default;

        virtual Result<void> put(util::nonnull<SUType *> su) override {
            auto e       = get_entity(su);
            e->state     = ThreadState::READY;
            e->slice_cnt = 0;
            ready_queue.push_back(*e);
            void_return();
        }

        virtual Result<void> insert(util::nonnull<SUType *> su) override {
            return put(su);
        }

        virtual Result<void> fetch(util::nonnull<SUType *> su) override {
            auto e = get_entity(su);
            if (!ready_queue.contains(*e)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            ready_queue.remove(*e);
            e->state = ThreadState::EMPTY;
            void_return();
        }

        virtual Result<void> remove(util::nonnull<SUType *> su) override {
            return fetch(su);
        }

        virtual Result<util::nonnull<SUType *>> pick() override {
            if (ready_queue.empty()) {
                unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
            }
            auto e = &ready_queue.front();
            auto su = get_su(e);
            return util::nonnull<SUType *>::from(su).transform_error(always(ErrCode::NULLPTR));
        }

        virtual Result<void> set_run(util::nonnull<SUType *> su) override {
            auto e = get_entity(su);
            e->state = ThreadState::RUNNING;
            e->slice_cnt = 0;
            void_return();
        }

        virtual Result<bool> runout(util::nonnull<const SUType *> su) override
        {
            if (ready_queue.empty()) {
                return false;
            }

            auto e = get_entity(su.get());

            if (e->slice_cnt >= TIME_SLICES) {
                return true;
            }

            // 仅在当前仍处于 RUNNING 且时间片未用尽时继续运行
            return e->state != ThreadState::RUNNING;
        }

        virtual void on_tick(util::nonnull<SUType *> su, units::tick /*gap_ticks*/) {
            auto e = get_entity(su);
            if (e->state == ThreadState::RUNNING) {
                e->slice_cnt++;
            }
        }
    };
}  // namespace schd::rr