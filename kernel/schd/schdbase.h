/**
 * @file schdbase.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief scheduler base
 * @version alpha-1.0.0
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/nonnull.h>
#include <sustcore/errcode.h>

enum class ThreadState { EMPTY = 0, READY = 1, RUNNING = 2, YIELD = 3 };

constexpr const char *to_string(ThreadState state) {
    switch (state) {
        case ThreadState::EMPTY:   return "EMPTY";
        case ThreadState::READY:   return "READY";
        case ThreadState::RUNNING: return "RUNNING";
        case ThreadState::YIELD:   return "YIELD";
        default:                   return "UNKNOWN";
    }
}

namespace schd {
    template <typename SU, typename Tags>
    class SchdBase {
    public:
        using SUType   = SU;
        using TagsType = Tags;

    protected:
        constexpr SchdBase()  = default;
        constexpr ~SchdBase() = default;

    public:
        /**
         * @brief 向调度器中添加一个SU
         *
         * @param su 调度单元
         */
        virtual Result<void> put(util::nonnull<SUType *> su) = 0;
        /**
         * @brief 从调度器中移除一个SU
         *
         * @param su 调度单元
         */
        virtual Result<void> fetch(util::nonnull<SUType *> su) = 0;
        /**
         * @brief 将一个SU插入调度器.
         *
         * @note 该函数与put的区别在于, insert添加的SU对调度器而言是全新的
         *
         * @param su 调度单元
         */
        virtual Result<void> insert(util::nonnull<SUType *> su)  = 0;
        /**
         * @brief 从调度器中移除一个SU
         *
         * @note 该函数与fetch的区别在于, remove后的SU对调度器而言是全新的
         *
         * @param su 调度单元
         */
        virtual Result<void> remove(util::nonnull<SUType *> su)  = 0;
        /**
         * @brief 从调度器中选择一个SU进行运行
         *
         * @return SUType* 选择的SU
         */
        virtual Result<util::nonnull<SUType *>> pick()           = 0;

        /**
         * @brief 将一个SU设置为正在运行状态
         *
         * 该函数一般与pick配合使用, 即:
         * auto su = pick();
         * fetch(su);
         * set_run(su);
         *
         * @param su 调度单元
         * @return Result<void> 设置结果
         */
        virtual Result<void> set_run(util::nonnull<SUType *> su) = 0;

        /**
         * @brief 判断当前运行的SU是否需要被切换掉
         *
         * @return true 需要切换
         * @return false 不需要切换
         */
        virtual Result<bool> runout(util::nonnull<const SUType *> su)  = 0;
    };
}  // namespace schd