/**
 * @file lock.cpp
 * @brief Demonstrate tay lock ownership and synchronized values.
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
    std::printf("ready=%s updates=%d\n", access->ready ? "true" : "false",
                access->updates);
    return 0;
}
