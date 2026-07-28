/**
 * @file refc.cpp
 * @brief Demonstrate tay::refc and tay::refc_ptr lifetime tracking.
 * @version 0.1.0-dev.1
 * @date 2026-07-28
 */

#include <cstdio>

#include <tay/refcount.h>

namespace {
    struct resource : tay::refc<resource> {
        bool released = false;

        void on_death() {
            released = true;
            std::puts("resource reached zero references");
        }
    };
}  // namespace

int main() {
    resource value;
    {
        tay::refc_ptr<resource> first{&value};
        std::printf("after first owner: %zu\n", value.ref_count());
        {
            tay::refc_ptr<resource> second{first};
            std::printf("after copy: %zu\n", value.ref_count());
        }
        std::printf("after inner scope: %zu\n", value.ref_count());
    }
    std::printf("released: %s\n", value.released ? "yes" : "no");
    return 0;
}
