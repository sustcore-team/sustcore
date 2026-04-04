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

#include <schd/hook.h>
#include <schd/schdbase.h>
#include <sus/list.h>
#include <sustcore/errcode.h>

namespace schd::fcfs {
    class Entity {
    public:
        ThreadState state                = ThreadState::EMPTY;
        util::ListHead<Entity> list_head = {};

        constexpr Entity()  = default;
        constexpr ~Entity() = default;
    };

    class tags : public schd::tags::empty {};

    template <typename SU>
    class FCFS : public SchdBase<SU, tags> {
    public:
        using SUType     = SU;
        using EntityType = Entity;
        using Tags       = tags;
        using Base       = SchdBase<SUType, Tags>;

        static_assert(
            requires(SUType su) { su.fcfs_entity; },
            "SUType须具有类型为schd::fcfs::Entity的成员fcfs_entity");
        constexpr static size_t ENTITY_OFFSET = offsetof(SUType, fcfs_entity);

        constexpr static EntityType *get_entity(SUType *su) {
            return &su->fcfs_entity;
        }

        constexpr static const EntityType *get_entity(const SUType *su) {
            return &su->fcfs_entity;
        }

        constexpr static SUType *get_su(EntityType *entity) {
            return reinterpret_cast<SUType *>(
                reinterpret_cast<char *>(entity) - ENTITY_OFFSET);
        }

    private:
        util::IntrusiveList<EntityType> ready_queue;
    public:
        constexpr FCFS()  = default;
        constexpr ~FCFS() = default;

        // 向就绪队列加入一个已知 SU
        virtual Result<void> put(util::nonnull<SUType *> su) override {
            auto e   = get_entity(su);
            e->state = ThreadState::READY;
            ready_queue.push_back(*e);
            void_return();
        }

        // 插入一个全新的 SU，与 put 语义相同
        virtual Result<void> insert(util::nonnull<SUType *> su) override {
            return put(su);
        }

        // 从就绪队列中移除一个 SU
        virtual Result<void> fetch(util::nonnull<SUType *> su) override {
            auto e = get_entity(su);
            if (!ready_queue.contains(*e)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            ready_queue.remove(*e);
            e->state = ThreadState::EMPTY;
            void_return();
        }

        // remove 语义与 fetch 相同
        virtual Result<void> remove(util::nonnull<SUType *> su) override {
            return fetch(su);
        }

        // 选择下一个要运行的 SU（不修改其状态与队列，仅返回引用）
        virtual Result<util::nonnull<SUType *>> pick() override {
            if (ready_queue.empty()) {
                unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
            }
            auto e  = &ready_queue.front();
            auto su = get_su(e);
            return util::nonnull<SUType *>::from(su).transform_error(always(ErrCode::NULLPTR));
        }

        // 将 SU 标记为 RUNNING
        virtual Result<void> set_run(util::nonnull<SUType *> su) override {
            auto e   = get_entity(su);
            e->state = ThreadState::RUNNING;
            void_return();
        }

        // FCFS: 只要当前仍处于 RUNNING，就不主动切换
        virtual Result<bool> runout(
            util::nonnull<const SUType *> su) override {
            if (ready_queue.empty()) {
                return false;
            }

            auto e = get_entity(su.get());
            return e->state != ThreadState::RUNNING;
        }
    };
}  // namespace schd::fcfs