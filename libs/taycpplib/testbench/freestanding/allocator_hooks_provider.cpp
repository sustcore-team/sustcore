#include <tay/allocator.h>

#include <cstddef>
#include <cstdint>

namespace {
    alignas(256) unsigned char storage[4096];
    std::size_t offset = 0;
}  // namespace

void* tay::__alloc(std::size_t size, std::size_t alignment) noexcept {
    const uintptr_t address = reinterpret_cast<uintptr_t>(storage + offset);
    const std::size_t padding =
        static_cast<std::size_t>(-address) & (alignment - 1);
    if (size > sizeof(storage) - offset ||
        padding > sizeof(storage) - offset - size)
    {
        return nullptr;
    }
    offset       += padding;
    void* result  = storage + offset;
    offset       += size;
    return result;
}

void tay::__free(void*, std::size_t, std::size_t) noexcept {}
