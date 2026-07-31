/**
 * @file guard.cpp
 * @brief Demonstrate scope cleanup and commit with tay::guard.
 */

#include <cstdio>

#include <tay/guard.h>

int main() {
    bool locked = true;
    {
        tay::guard unlock{[&] {
            locked = false;
            std::puts("automatic cleanup: unlocked");
        }};

        std::puts("working while locked");
    }

    tay::guard rollback{[] { std::puts("rollback"); }};
    rollback.release();
    std::puts(locked ? "still locked" : "cleanup completed");
    return 0;
}
