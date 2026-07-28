/**
 * @file owner.cpp
 * @brief Demonstrate explicit raw-pointer ownership with tay::owner.
 * @version 0.1.0-dev.1
 * @date 2026-07-28
 */

#include <cstdio>

#include <tay/owner.h>

namespace {
    struct record {
        int id;
    };
}  // namespace

int main() {
    tay::owner owned{new record{42}};
    std::printf("owned record id: %d\n", owned->id);

    // tay::owner is an ownership annotation, so destruction remains explicit.
    delete owned.get();
    return 0;
}
