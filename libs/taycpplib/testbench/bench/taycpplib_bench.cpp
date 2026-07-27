#include <chrono>
#include <cstdint>
#include <cstdio>
#include <tay/algobase.h>
#include <tay/range.h>

int main() {
    constexpr std::uint64_t iterations = 10000000;
    auto start = std::chrono::steady_clock::now();
    const auto seed = static_cast<std::uint64_t>(start.time_since_epoch().count());
    volatile std::uint64_t checksum = 0;
    for (std::uint64_t i = 0; i < iterations; ++i) {
        const auto value = i + seed;
        tay::range<std::uint64_t> a{value, value + 32};
        tay::range<std::uint64_t> b{value + 8, value + 48};
        auto overlap = tay::intersection(a, b);
        checksum = checksum + overlap.size() + tay::clamp(value, value / 2, value + 1);
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("taycpplib range: iterations=%llu elapsed=%lld ns ns/op=%.2f checksum=%llu\n",
                (unsigned long long)iterations, (long long)elapsed,
                (double)elapsed / (double)iterations,
                (unsigned long long)checksum);
    return 0;
}
