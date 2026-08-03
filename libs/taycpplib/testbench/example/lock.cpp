/**
 * @file lock.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 Tay 锁所有权和同步值工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/lock.h>

#include <cstdio>

namespace {
    struct device_state {
        bool ready  = false;
        int updates = 0;
    };
}  // namespace

int main() {
    tay::spinlock output_lock;
    {
        tay::lock_guard held{output_lock};
        std::puts("lock_guard owns the output lock");
    }

    tay::unique_lock deferred{output_lock, tay::defer_lock};
    deferred.lock();
    std::puts("unique_lock can release before scope exit");
    deferred.unlock();

    tay::synchronized<device_state> state;
    {
        auto access   = state.lock();
        access->ready = true;
        ++access->updates;
    }

    const auto& readonly = state;
    auto access          = readonly.lock();
    std::printf("ready=%s updates=%d\n", access->ready ? "true" : "false", access->updates);
    return 0;
}
