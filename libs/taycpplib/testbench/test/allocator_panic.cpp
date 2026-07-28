#include <tay/allocator.h>

#include <cstddef>

template <class T>
struct failing_allocator {
    using value_type = T;

    tay::expected<T *, tay::error_code> try_allocate(std::size_t) noexcept {
        return tay::expected<T *, tay::error_code>(
            tay::unexpect, tay::error_code::OUT_OF_MEMORY);
    }

    void deallocate(T *, std::size_t) noexcept {}
};

int main() {
    failing_allocator<int> allocator;
    (void)tay::allocator_traits<failing_allocator<int>>::allocate(allocator, 1);
}
