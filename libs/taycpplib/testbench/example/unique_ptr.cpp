/**
 * @file unique_ptr.cpp
 * @brief Demonstrate scalar, array, and owner interoperation.
 */

#include <cstdio>
#include <utility>

#include <tay/owner.h>
#include <tay/unique_ptr.h>

namespace {
    struct record {
        int id;
    };
}  // namespace

int main() {
    auto item = tay::make_unique<record>(record{42});
    std::printf("record id: %d\n", item->id);

    tay::unique_ptr<record> moved{std::move(item)};
    auto owned = moved.release_owner();
    tay::unique_ptr<record> adopted{std::move(owned)};
    std::printf("adopted id: %d\n", adopted->id);

    auto values = tay::make_unique<int[]>(4);
    for (std::size_t i = 0; i < 4; ++i) {
        values[i] = static_cast<int>(i * i);
    }
    std::printf("array: %d %d %d %d\n", values[0], values[1], values[2],
                values[3]);
    return 0;
}
