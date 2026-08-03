/**
 * @file allocator.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 tay::allocator 的分配和释放用法。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/allocator.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/string.h>

#include <cstddef>
#include <cstdio>
#include <type_traits>
#include <utility>

namespace {
    struct allocation_stats {
        size_t allocations   = 0;
        size_t deallocations = 0;
        size_t bytes_in_use  = 0;
    };

    struct counting_allocator_state {
        inline static allocation_stats* stats = nullptr;
    };

    template <class T>
    class counting_allocator {
    public:
        using value_type      = T;
        using pointer         = T*;
        using const_pointer   = const T*;
        using size_type       = size_t;
        using difference_type = std::ptrdiff_t;
        using is_always_equal = std::true_type;

        constexpr counting_allocator() noexcept = default;

        template <class U>
        constexpr counting_allocator(const counting_allocator<U>&) noexcept {}

        template <class U>
        struct rebind {
            using other = counting_allocator<U>;
        };

        static void bind(allocation_stats& stats) noexcept {
            counting_allocator_state::stats = &stats;
        }

        // tay containers use this non-panicking allocation entry point.
        [[nodiscard]]
        tay::expected<T*, tay::error_code> try_allocate(size_type count) noexcept {
            tay::allocator<T> upstream;
            auto result = upstream.try_allocate(count);
            if (result) {
                ++counting_allocator_state::stats->allocations;
                counting_allocator_state::stats->bytes_in_use += count * sizeof(T);
            }
            return result;
        }

        // The standard-compatible entry point keeps the allocator usable by
        // generic code; tay::allocator_traits supplies its panic policy.
        [[nodiscard]]
        pointer allocate(size_type count) noexcept {
            return tay::allocator_traits<counting_allocator>::allocate(*this, count);
        }

        // A tay allocator's deallocation path must also be noexcept.
        void deallocate(T* memory, size_type count) noexcept {
            if (memory == nullptr) {
                return;
            }
            ++counting_allocator_state::stats->deallocations;
            counting_allocator_state::stats->bytes_in_use -= count * sizeof(T);
            tay::allocator<T> upstream;
            upstream.deallocate(memory, count);
        }

        [[nodiscard]]
        constexpr size_type max_size() const noexcept {
            return tay::allocator<T>{}.max_size();
        }

        template <class U>
        friend constexpr bool operator==(const counting_allocator&,
                                         const counting_allocator<U>&) noexcept {
            return true;
        }
    };

    void print_stats(const char* label, const allocation_stats& stats) {
        std::printf("%-22s allocations=%zu, deallocations=%zu, in-use=%zu B\n", label,
                    stats.allocations, stats.deallocations, stats.bytes_in_use);
    }
}  // namespace

int main() {
    allocation_stats stats;
    counting_allocator<char>::bind(stats);
    using string = tay::string<counting_allocator<char>>;
    static_assert(std::is_empty_v<counting_allocator<char>>);
    static_assert(counting_allocator<char>{} == counting_allocator<char>{});

    print_stats("initial", stats);
    {
        // string default-constructs its stateless allocator.
        auto created = string::try_create("short");
        if (!created) {
            std::printf("creation failed with error code %u\n",
                        static_cast<unsigned>(created.error()));
            return 1;
        }
        string text = std::move(*created);

        // The allocator is stored in the string, but short text uses SSO.
        print_stats("after short string", stats);

        // Recoverable operations return tay::expected on allocation failure.
        auto reserved = text.reserve(64);
        if (!reserved) {
            std::printf("reserve failed with error code %u\n",
                        static_cast<unsigned>(reserved.error()));
            return 1;
        }
        print_stats("after reserve(64)", stats);

        auto appended = text.append(" text managed by a custom allocator");
        if (!appended) {
            std::printf("append failed with error code %u\n",
                        static_cast<unsigned>(appended.error()));
            return 1;
        }
        std::printf("text: %s\n", text.c_str());
        print_stats("before destruction", stats);
    }
    print_stats("after destruction", stats);
    return 0;
}
