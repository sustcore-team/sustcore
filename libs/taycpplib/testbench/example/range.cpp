/**
 * @file range.cpp
 * @brief Demonstrate tay::range intersection and containment.
 * @version 0.1.0-dev.1
 * @date 2026-07-28
 */

#include <cstdio>

#include <tay/range.h>

int main() {
    constexpr tay::range<int> available{10, 30};
    constexpr tay::range<int> requested{20, 40};
    constexpr auto overlap = tay::intersection(available, requested);

    std::printf("available: [%d, %d)\n", available.begin, available.end);
    std::printf("requested: [%d, %d)\n", requested.begin, requested.end);
    std::printf("overlap: [%d, %d), size=%zu\n", overlap.begin, overlap.end,
                overlap.size());
    std::printf("25 is available: %s\n",
                tay::within(available, 25) ? "yes" : "no");
    return 0;
}
