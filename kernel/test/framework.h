/**
 * @file framework.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核分阶段 selftest 的统一执行接口。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

namespace task {
    class Thread;
}

namespace kernel::test {
    /** @brief selftest 可运行的内核初始化阶段。 */
    enum class Phase {
        POST_TIMER_INITIALIZATION,
        POST_SCHEDULER_INITIALIZATION,
        POST_WORK_QUEUE_INITIALIZATION,
        PRE_IDLE,
    };

    /** @brief 由生产启动路径显式提供给当前阶段测试的运行时对象。 */
    struct Context {
        task::Thread *current_thread = nullptr;
    };

    /** @brief 按注册顺序运行指定阶段的全部 selftest。 */
    void run_phase(Phase phase, Context context = {}) noexcept;

    /** @brief 报告当前用例失败并终止内核。 */
    [[noreturn]] void fail(const char *message) noexcept;

    /** @brief 验证当前用例条件，不满足时调用 fail()。 */
    void require(bool condition, const char *message) noexcept;
}  // namespace kernel::test
