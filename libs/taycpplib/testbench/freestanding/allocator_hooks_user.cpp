#include <tay/format.h>

#include <cstdint>

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
    if (!aligned ||
        reinterpret_cast<uintptr_t>(*aligned) % alignof(over_aligned) != 0)
    {
        return 1;
    }
    allocator.deallocate(*aligned, 1);

    auto output = tay::format(
        "freestanding owning format requires allocator hooks: {}", 42);
    return output.has_value() ? 0 : 1;
}
