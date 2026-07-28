#include <tay/allocator.h>
#include <tay/string.h>

#include <cstddef>
#include <type_traits>

template <class T>
struct freestanding_allocator {
    using value_type = T;

    constexpr tay::expected<T *, tay::error_code> try_allocate(
        std::size_t) noexcept {
        return tay::expected<T *, tay::error_code>(
            tay::unexpect, tay::error_code::OUT_OF_MEMORY);
    }

    constexpr void deallocate(T *, std::size_t) noexcept {}
};

using test_string = tay::string<freestanding_allocator<char>>;

static_assert(
    std::is_nothrow_default_constructible_v<freestanding_allocator<char>>);
static_assert(
    std::is_same_v<
        decltype(tay::allocator_traits<freestanding_allocator<int>>::
                     try_allocate(std::declval<freestanding_allocator<int> &>(),
                                  1)),
        tay::expected<int *, tay::error_code>>);
static_assert(sizeof(void *) != 8 || sizeof(test_string) == 32);

void freestanding_string_contract() {
    test_string text("tay");
    (void)text.append(" string");
    (void)text.at(20);
    auto failed =
        test_string::try_create("this string requires dynamic storage");
    (void)failed;
}
