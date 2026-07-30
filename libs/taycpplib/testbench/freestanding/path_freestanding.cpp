#include <tay/path.h>

#include <type_traits>

static_assert(
    std::is_same_v<typename tay::path<>::allocator_type, tay::allocator<char>>);

void path_contract() {
    auto path = tay::path<>::try_create("/boot/../kernel");
    if (!path) {
        return;
    }
    auto normalized = path->try_normalize();
    auto relative   = path->try_relative_to(*path);
    (void)normalized;
    (void)relative;
}
