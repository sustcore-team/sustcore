#include <tay/array_list.h>
#include <tay/map.h>
#include <tay/set.h>

#include <cstddef>
#include <type_traits>
#include <utility>

template <class T>
struct freestanding_allocator {
    using value_type = T;

    template <class U>
    constexpr freestanding_allocator(
        const freestanding_allocator<U> &) noexcept {}
    constexpr freestanding_allocator() noexcept = default;

    constexpr tay::expected<T *, tay::error_code> try_allocate(
        std::size_t) noexcept {
        return tay::expected<T *, tay::error_code>(
            tay::unexpect, tay::error_code::OUT_OF_MEMORY);
    }
    constexpr void deallocate(T *, std::size_t) noexcept {}

    template <class U>
    struct rebind {
        using other = freestanding_allocator<U>;
    };
};

template <class T, class U>
constexpr bool operator==(const freestanding_allocator<T> &,
                          const freestanding_allocator<U> &) noexcept {
    return true;
}

using map_value = std::pair<const int, int>;
using list_type = tay::array_list<int, freestanding_allocator<int>>;
using map_type  = tay::hash_map<int, int, std::hash<int>, std::equal_to<int>,
                                freestanding_allocator<map_value>>;
using set_type  = tay::hash_set<int, std::hash<int>, std::equal_to<int>,
                                freestanding_allocator<int>>;

static_assert(std::is_same_v<decltype(map_type::try_create()),
                             tay::expected<map_type, tay::error_code>>);
static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(*std::declval<set_type::iterator>())>>);

void freestanding_container_contract() {
    list_type list;
    (void)list.push_back(1);
    auto map = map_type::try_create();
    auto set = set_type::try_create();
    (void)map;
    (void)set;
}
