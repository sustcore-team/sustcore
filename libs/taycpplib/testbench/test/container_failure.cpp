#include <tay/list.h>
#include <tay/map.h>
#include <tay/set.h>

#include <cassert>
#include <cstddef>
#include <new>

namespace {
    struct allocation_state {
        bool fail = false;
    };

    template <class T>
    struct checked_allocator {
        using value_type      = T;
        using is_always_equal = std::false_type;

        allocation_state* state;

        explicit checked_allocator(allocation_state& shared) noexcept
            : state(&shared) {}

        template <class U>
        checked_allocator(const checked_allocator<U>& other) noexcept
            : state(other.state) {}

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
            return memory;
        }

        void deallocate(T* memory, std::size_t) noexcept {
            ::operator delete(memory);
        }

        template <class U>
        struct rebind {
            using other = checked_allocator<U>;
        };

        template <class>
        friend struct checked_allocator;
    };

    template <class T, class U>
    bool operator==(const checked_allocator<T>& left,
                    const checked_allocator<U>& right) noexcept {
        return left.state == right.state;
    }

    template <class T, class U>
    bool operator!=(const checked_allocator<T>& left,
                    const checked_allocator<U>& right) noexcept {
        return !(left == right);
    }

    template <class Map>
    concept has_subscript = requires(Map& map) { map[0]; };
}  // namespace

int main() {
    allocation_state state;
    checked_allocator<int> list_allocator(state);
    tay::list<int, checked_allocator<int>> list(list_allocator);
    state.fail       = true;
    auto list_failed = list.push_back(1);
    assert(!list_failed);
    assert(list_failed.error() == tay::error_code::OUT_OF_MEMORY);
    assert(list.empty() && list.capacity() == 0);

    using map_value = std::pair<const int, int>;
    using map_type  = tay::map<int, int, checked_allocator<map_value>>;
    static_assert(!has_subscript<map_type>);

    checked_allocator<map_value> map_allocator(state);
    auto create_failed = map_type::try_create(map_allocator);
    assert(!create_failed);
    assert(create_failed.error() == tay::error_code::OUT_OF_MEMORY);

    state.fail       = false;
    auto map_created = map_type::try_create(map_allocator);
    assert(map_created);
    assert(map_created->try_emplace(1, 10));
    const auto old_buckets = map_created->bucket_count();

    state.fail       = true;
    auto node_failed = map_created->try_emplace(2, 20);
    assert(!node_failed);
    assert(node_failed.error() == tay::error_code::OUT_OF_MEMORY);
    assert(map_created->size() == 1 && map_created->contains(1));
    assert(!map_created->contains(2));

    auto rehash_failed = map_created->rehash(old_buckets + 16);
    assert(!rehash_failed);
    assert(rehash_failed.error() == tay::error_code::OUT_OF_MEMORY);
    assert(map_created->bucket_count() == old_buckets);
    assert(map_created->contains(1));

    using set_type  = tay::set<int, checked_allocator<int>>;
    auto set_failed = set_type::try_create(list_allocator);
    assert(!set_failed);
    assert(set_failed.error() == tay::error_code::OUT_OF_MEMORY);
    return 0;
}
