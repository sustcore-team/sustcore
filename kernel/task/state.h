/**
 * @file state.h
 * @brief 定义 Process 与 Thread 公共生命周期状态快照。
 */

#pragma once

#include <tay/bits.h>

namespace task {
    enum class ProcessState : u8_t {
        CREATED,
        SUBMITTED,
        STOPPING,
        DEAD,
    };

    enum class ThreadState : u8_t {
        CREATED,
        SUSPENDED,
        READY,
        RUNNING,
        BLOCKING,
        BLOCKED,
        EXITED,
    };

    enum class ThreadMode : u8_t {
        KERNEL,
        USER,
    };

    enum class TimedWaitState : u8_t {
        IDLE,
        PENDING,
        COMPLETED,
    };

    enum class TimedWaitResult : u8_t {
        NONE,
        TIMEOUT,
        WOKEN,
        CANCELLED,
    };
}  // namespace task
