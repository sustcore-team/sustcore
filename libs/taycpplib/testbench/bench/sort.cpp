#include <tay/algo/sort.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>

namespace {
    constexpr std::size_t batch_size = 256;

    template <std::size_t Size>
    void benchmark(const char* name, std::uint64_t iterations,
                   std::uint64_t seed) {
        volatile std::uint64_t checksum = 0;
        std::uint64_t state             = seed;
        std::int64_t elapsed            = 0;
        std::uint32_t values[batch_size][Size];

        for (std::uint64_t offset  = 0; offset < iterations;
             offset               += batch_size)
        {
            const auto remaining = iterations - offset;
            const auto count = remaining < batch_size ? remaining : batch_size;

            for (std::size_t item = 0; item < count; ++item) {
                for (std::size_t index = 0; index < Size; ++index) {
                    state = state * 6364136223846793005ULL + 1;
                    values[item][index] =
                        static_cast<std::uint32_t>(state >> 32);
                }
            }

            const auto start = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < count; ++item) {
                tay::sort(values[item]);
            }
            elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();

            for (std::size_t item = 0; item < count; ++item) {
                checksum = checksum + values[item][0] + values[item][Size - 1];
            }
        }

        std::cout << std::format(
            "{:28} size={:6d} iterations={:8d} elapsed={:11d} ns "
            "ns/sort={:9.2f} ns/element={:7.3f} checksum={}\n",
            name, (unsigned long long)Size, (unsigned long long)iterations,
            (long long)elapsed, (double)elapsed / (double)iterations,
            (double)elapsed / (double)(iterations * Size),
            (unsigned long long)checksum);
    }
}  // namespace

int main() {
    const auto seed = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    std::printf("taycpplib sort benchmark\n");
    benchmark<8>("sort-8", 1000000, seed);
    benchmark<32>("sort-32", 200000, seed);
    benchmark<128>("sort-128", 20000, seed);
    benchmark<1024>("sort-1024", 5000, seed);

    return 0;
}
