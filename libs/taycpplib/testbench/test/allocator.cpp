#include <tay/allocator.h>
#include <tay/array_list.h>
#include <tay/map.h>
#include <tay/set.h>
#include <tay/string.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace {
    struct allocation_state {
        bool fail                 = false;
        std::size_t allocations   = 0;
        std::size_t deallocations = 0;
    };

    template <class T>
    struct test_allocator {
        using value_type      = T;
        using is_always_equal = std::false_type;

        allocation_state* state = nullptr;

        template <class U>
        constexpr test_allocator(const test_allocator<U>& other) noexcept
            : state(other.state) {}

        constexpr explicit test_allocator(allocation_state& shared) noexcept
            : state(&shared) {}

        tay::expected<T*, tay::error_code> try_allocate(
            std::size_t count) noexcept {
            if (state->fail) {
                return tay::expected<T*, tay::error_code>(
                    tay::unexpect, tay::error_code::OUT_OF_MEMORY);
            }
            auto* memory = static_cast<T*>(
                ::operator new(count * sizeof(T), std::nothrow));
            if (memory == nullptr) {
                return tay::expected<T*, tay::error_code>(
                    tay::unexpect, tay::error_code::OUT_OF_MEMORY);
            }
            ++state->allocations;
            return memory;
        }

        void deallocate(T* memory, std::size_t) noexcept {
            ++state->deallocations;
            ::operator delete(memory);
        }

        template <class U>
        struct rebind {
            using other = test_allocator<U>;
        };

        template <class>
        friend struct test_allocator;
    };

    template <class T, class U>
    constexpr bool operator==(const test_allocator<T>& left,
                              const test_allocator<U>& right) noexcept {
        return left.state == right.state;
    }

    template <class T, class U>
    constexpr bool operator!=(const test_allocator<T>& left,
                              const test_allocator<U>& right) noexcept {
        return !(left == right);
    }

    struct object {
        int value;
        explicit object(int initial) noexcept : value(initial) {}
    };

    struct alignas(128) over_aligned {
        int value;
    };

    consteval bool constexpr_allocation_works() {
        tay::allocator<int> allocator;
        auto memory = allocator.try_allocate(2);
        if (!memory) {
            return false;
        }
        std::construct_at(*memory, 17);
        std::construct_at(*memory + 1, 25);
        const bool valid = (*memory)[0] + (*memory)[1] == 42;
        std::destroy_at(*memory);
        std::destroy_at(*memory + 1);
        allocator.deallocate(*memory, 2);
        return valid;
    }
}  // namespace

static_assert(std::is_same_v<
              tay::allocator_traits<tay::allocator<int>>::rebind_alloc<char>,
              tay::allocator<char>>);
static_assert(std::is_empty_v<tay::allocator<int>>);
static_assert(std::is_nothrow_default_constructible_v<tay::allocator<int>>);
static_assert(
    tay::allocator_traits<tay::allocator<int>>::is_always_equal::value);
static_assert(tay::allocator<int>{} == tay::allocator<int>{});
static_assert(std::is_same_v<
              decltype(tay::allocator_traits<tay::allocator<int>>::try_allocate(
                  std::declval<tay::allocator<int>&>(), 1)),
              tay::expected<int*, tay::error_code>>);
static_assert(constexpr_allocation_works());
static_assert(std::is_same_v<tay::string<>,
                             tay::string<tay::allocator<char>>>);
static_assert(std::is_same_v<tay::array_list<int>,
                             tay::array_list<int, tay::allocator<int>>>);
static_assert(std::is_same_v<tay::array_list<int>,
                             tay::array_list<int, tay::allocator<int>>>);
static_assert(std::is_same_v<tay::hash_set<int>,
                             tay::hash_set<int, std::hash<int>,
                                           std::equal_to<int>,
                                           tay::allocator<int>>>);
using default_map_value = std::pair<const int, int>;
static_assert(std::is_same_v<
              tay::hash_map<int, int>,
              tay::hash_map<int, int, std::hash<int>, std::equal_to<int>,
                            tay::allocator<default_map_value>>>);

int main() {
    tay::allocator<int> host_allocator;
    using host_traits = tay::allocator_traits<tay::allocator<int>>;

    auto zero = host_traits::try_allocate(host_allocator, 0);
    assert(zero && *zero == nullptr);

    auto memory = host_traits::try_allocate(host_allocator, 4);
    assert(memory && *memory != nullptr);
    host_traits::deallocate(host_allocator, *memory, 4);

    tay::allocator<over_aligned> aligned_allocator;
    auto aligned = aligned_allocator.try_allocate(1);
    assert(aligned);
    assert(reinterpret_cast<std::uintptr_t>(*aligned) % alignof(over_aligned) ==
           0);
    aligned_allocator.deallocate(*aligned, 1);

    auto overflow = host_traits::try_allocate(
        host_allocator, host_traits::max_size(host_allocator) + 1);
    assert(!overflow);
    assert(overflow.error() == tay::error_code::ALLOCATION_SIZE_OVERFLOW);

    allocation_state state;
    test_allocator<object> allocator(state);
    using traits = tay::allocator_traits<test_allocator<object>>;

    auto allocated = traits::try_allocate_at_least(allocator, 2);
    assert(allocated);
    assert(allocated->count == 2);
    assert(state.allocations == 1);

    traits::construct(allocator, allocated->ptr, 42);
    assert(allocated->ptr->value == 42);
    traits::destroy(allocator, allocated->ptr);
    traits::deallocate(allocator, allocated->ptr, allocated->count);
    assert(state.deallocations == 1);

    state.fail  = true;
    auto failed = traits::try_allocate(allocator, 1);
    assert(!failed);
    assert(failed.error() == tay::error_code::OUT_OF_MEMORY);
    return 0;
}
