/**
 * @file refc.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::refc 和 tay::refc_ptr 的生命周期跟踪。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 */

#include <tay/refcount.h>

#include <cstdio>

namespace {
    struct resource : tay::refc<resource> {
        bool released = false;

        void on_death() noexcept {
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
