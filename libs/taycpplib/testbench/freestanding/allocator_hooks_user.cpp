/**
 * @file allocator_hooks_user.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 freestanding 环境中自定义分配器钩子的使用。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/bits.h>
#include <tay/format.h>

namespace {
    struct alignas(256) over_aligned {
        char value;
    };
}  // namespace

int main() {
    tay::allocator<over_aligned> allocator;
    auto zero = allocator.try_allocate(0);
    if (!zero || *zero != nullptr) {
        return 1;
    }
    auto aligned = allocator.try_allocate(1);
    if (!aligned || reinterpret_cast<uintptr_t>(*aligned) % alignof(over_aligned) != 0) {
        return 1;
    }
    allocator.deallocate(*aligned, 1);

    auto output = tay::format("freestanding owning format requires allocator hooks: {}", 42);
    return output.has_value() ? 0 : 1;
}
